#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Sprites.h"
#include "Textures.h"
#include "Things/MapObj.h"

static constexpr Fixed computeStretch(const uint32_t width, const uint32_t height) noexcept {
    return Fixed(
        floatToFixed((160.0f / (float) width) * ((float) height / 180.0f) * 2.2f)
    );
}

static constexpr uint32_t SCREEN_WIDTHS[6] = { 280, 256, 224, 192, 160, 128 };
static constexpr uint32_t SCREEN_HEIGHTS[6] = { 160, 144, 128, 112, 96, 80 };

static constexpr Fixed STRETCHES[6] = {
    computeStretch(280, 160),
    computeStretch(256, 144),
    computeStretch(224, 128),
    computeStretch(192, 112),
    computeStretch(160, 96),
    computeStretch(128, 80)
};

BEGIN_NAMESPACE(Renderer)

//----------------------------------------------------------------------------------------------------------------------
// Internal renderer cross module globals
//----------------------------------------------------------------------------------------------------------------------
viswall_t       gVisWalls[MAXWALLCMDS];
viswall_t*      gpEndVisWall;
visplane_t      gVisPlanes[MAXVISPLANES];
visplane_t*     gpEndVisPlane;
vissprite_t     gVisSprites[MAXVISSPRITES];
vissprite_t*    gpEndVisSprite;
uint8_t         gOpenings[MAXOPENINGS];
uint8_t*        gpEndOpening;
Fixed           gViewX;
Fixed           gViewY;
Fixed           gViewZ;
angle_t         gViewAngle;
Fixed           gViewCos;
Fixed           gViewSin;
uint32_t        gExtraLight;
angle_t         gClipAngle;
angle_t         gDoubleClipAngle;
uint32_t        gSprOpening[MAXSCREENWIDTH];
int32_t         gTexScale;

//----------------------------------------------------------------------------------------------------------------------
// Load in the "TextureInfo" array so that the game knows all about the wall and sky textures (Width,Height).
// Also initialize the texture translation table for wall animations. 
// Called on startup only.
//----------------------------------------------------------------------------------------------------------------------
static void initData() noexcept {
    // Initialize render asset managers
    texturesInit();
    spritesInit();
    
    // Create a recipocal mul table so that I can divide 0-8191 from 1.0.
    // This way I can fake a divide with a multiply.
    gIDivTable[0] = -1;
    
    {
        uint32_t i = 1;
        
        do {
            gIDivTable[i] = fixedDiv(512 << FRACBITS, i << FRACBITS);    // 512.0 / i
        } while (++i < (sizeof(gIDivTable) / sizeof(uint32_t)));
    }
    
    // First time init of the math tables.
    // They may change however if the view size changes!
    initMathTables();
}

//----------------------------------------------------------------------------------------------------------------------
// Sets up various things prior to rendering for the new frame
//----------------------------------------------------------------------------------------------------------------------
static void preDrawSetup() noexcept {
    const player_t& player = gPlayers;      // Which player is the camera attached?
    const mobj_t& mapObj = *player.mo;
    gViewX = mapObj.x;                      // Get the position of the player
    gViewY = mapObj.y;
    gViewZ = player.viewz;                  // Get the height of the player
    gViewAngle = mapObj.angle;              // Get the angle of the player

    {
        const uint32_t angleIdx = gViewAngle >> ANGLETOFINESHIFT;   // Precalc angle index
        gViewSin = gFineSine[angleIdx];                             // Get the base sine value
        gViewCos = gFineCosine[angleIdx];                           // Get the base cosine value
    }

    gExtraLight = player.extralight << 6;   // Init the extra lighting value
    gpEndVisPlane = gVisPlanes + 1;         // visplanes[0] is left empty
    gpEndVisWall = gVisWalls;               // No walls added yet
    gpEndVisSprite = gVisSprites;           // No sprites added yet
    gpEndOpening = gOpenings;               // No openings found
}

void init() noexcept {
    initData();                             // Init resource managers and all of the lookup tables
    gClipAngle = gXToViewAngle[0];          // Get the left clip angle from viewport
    gDoubleClipAngle = gClipAngle * 2;      // Precalc angle * 2
}

void initMathTables() noexcept {
    gScreenWidth = SCREEN_WIDTHS[gScreenSize];
    gScreenHeight = SCREEN_HEIGHTS[gScreenSize];
    gCenterX = gScreenWidth / 2;
    gCenterY = gScreenHeight / 2;
    gScreenXOffset = (320 - gScreenWidth) / 2;
    gScreenYOffset = (160 - gScreenHeight) / 2;
    gGunXScale = (gScreenWidth * 0x100000) / 320;       // Get the 3DO scale factor for the gun shape and the y scale
    gGunYScale = (gScreenHeight * 0x10000) / 160;
    gStretch = STRETCHES[gScreenSize];
    gStretchWidth = gStretch * ((int) gScreenWidth / 2);

    // Create the 'view angle to x' table
    {
        const Fixed j = fixedDiv(gCenterX << FRACBITS, gFineTangent[FINEANGLES / 4 + FIELDOFVIEW / 2]);

        for (uint32_t i = 0; i < FINEANGLES / 2; i += 2) {
            Fixed t;
            if (gFineTangent[i] > FRACUNIT * 2) {
                t = -1;
            } else if (gFineTangent[i] < -FRACUNIT * 2) {
                t = gScreenWidth + 1;
            } else {
                t = fixedMul(gFineTangent[i], j);
                t = ((gCenterX << FRACBITS) - t + FRACUNIT - 1) >> FRACBITS;
                if (t < -1) {
                    t = -1;
                } else if ( t > (int) gScreenWidth + 1) {
                    t = gScreenWidth + 1;
                }
            }
            gViewAngleToX[i / 2] = t;
        }
    }
    
    // Using the 'view angle to x' table, create 'x to view angle' table
    for (uint32_t i = 0; i <= gScreenWidth; ++i) {
        uint32_t x = 0;
        while (gViewAngleToX[x] > (int) i) {
            ++x;
        }
        gXToViewAngle[i] = (x << (ANGLETOFINESHIFT + 1)) - ANG90;
    }
    
    // Set the minimums and maximums for 'view angle to x' 
    for (uint32_t i = 0; i < FINEANGLES / 4; ++i) {
        if (gViewAngleToX[i] == -1) {
            gViewAngleToX[i] = 0;
        } else if (gViewAngleToX[i] == gScreenWidth + 1) {
            gViewAngleToX[i] = gScreenWidth;
        }
    }
    
    // Make the 'y slope' table for floor and ceiling textures
    for (uint32_t i = 0; i < gScreenHeight; ++i) {
        Fixed j = (((int) i - (int) gScreenHeight / 2) * FRACUNIT) + FRACUNIT / 2;
        j = fixedDiv(gStretchWidth, abs(j));
        j >>= 6;

        if (j > 0xFFFF) {
            j = 0xFFFF;
        }

        gYSlope[i] = j;
    }

    // Create the distance scale table for floor and ceiling textures 
    for (uint32_t i = 0; i < gScreenWidth; ++i) {
        const Fixed j = std::abs(gFineCosine[gXToViewAngle[i] >> ANGLETOFINESHIFT]);
        gDistScale[i] = fixedDiv(FRACUNIT, j) >> 1;
    }

    // Create the lighting tables
    for (uint32_t i = 0; i < 256; ++i) {
        const Fixed j = i / 3;
        gLightMins[i] = j;      // Save the light minimum factors

        const Fixed Range = i - j;
        gLightSubs[i] = ((Fixed) gScreenWidth * Range) / (800 - (Fixed) gScreenWidth);
        gLightCoefs[i] = (Range << 16) / (800 - (Fixed) gScreenWidth);
        gPlaneLightCoef[i] = Range * (0x140000 / (800 - (Fixed) gScreenWidth));
    }
}

void drawPlayerView() noexcept {
    preDrawSetup();                 // Init variables based on camera angle
    doBspTraversal();               // Traverse the BSP tree and build lists of walls, floors (visplanes) and sprites to render
    drawAllLineSegs();              // Draw all everything Z Sorted
    drawAllVisPlanes();
    drawAllMapObjectSprites();
    DrawColors();                   // Draw color overlay if needed    
    DrawWeapons();                  // Draw the weapons on top of the screen
}

END_NAMESPACE(Renderer)
