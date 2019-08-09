#include "Renderer_Internal.h"

#include "Base/FMath.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Sprites.h"
#include "Textures.h"
#include "Things/MapObj.h"
#include <algorithm>

static constexpr Fixed computeStretch(const uint32_t width, const uint32_t height) noexcept {
    return Fixed(
        floatToFixed(
            (160.0f / (float) width) *
            ((float) height / (float) Renderer::REFERENCE_3D_VIEW_HEIGHT) *
            2.2f
        )
    );
}

static constexpr uint32_t SCREEN_WIDTHS[6] = {
// TODO: REMOVE
#if HACK_TEST_HIGH_RES_RENDERING
    280 * HACK_TEST_HIGH_RENDER_SCALE,
#else
    280,
#endif
    256,
    224,
    192,
    160,
    128
};

static constexpr uint32_t SCREEN_HEIGHTS[6] = {
// TODO: REMOVE
#if HACK_TEST_HIGH_RES_RENDERING
    160 * HACK_TEST_HIGH_RENDER_SCALE,
#else
    160,
#endif
    144,
    128,
    112,
    96,
    80
};

static constexpr Fixed STRETCHES[6] = {
// TODO: REMOVE
#if HACK_TEST_HIGH_RES_RENDERING
    computeStretch(280 * HACK_TEST_HIGH_RENDER_SCALE, 160 * HACK_TEST_HIGH_RENDER_SCALE),
#else
    computeStretch(280, 160),
#endif
    computeStretch(256, 144),
    computeStretch(224, 128),
    computeStretch(192, 112),
    computeStretch(160, 96),
    computeStretch(128, 80)
};

BEGIN_NAMESPACE(Renderer)

#if ENABLE_DEBUG_CAMERA_Z_MOVEMENT
    float gDebugCameraZOffset;
#endif

//----------------------------------------------------------------------------------------------------------------------
// Internal renderer cross module globals
//----------------------------------------------------------------------------------------------------------------------
viswall_t                       gVisWalls[MAXWALLCMDS];
viswall_t*                      gpEndVisWall;
visplane_t                      gVisPlanes[MAXVISPLANES];
visplane_t*                     gpEndVisPlane;
vissprite_t                     gVisSprites[MAXVISSPRITES];
vissprite_t*                    gpEndVisSprite;
uint8_t                         gOpenings[MAXOPENINGS];
uint8_t*                        gpEndOpening;
std::vector<DrawSeg>            gDrawSegs;
Fixed                           gViewXFrac;
Fixed                           gViewYFrac;
Fixed                           gViewZFrac;
float                           gViewX;
float                           gViewY;
float                           gViewZ;
float                           gViewDirX;
float                           gViewDirY;
float                           gViewPerpX;
float                           gViewPerpY;
angle_t                         gViewAngleBAM;
float                           gViewAngle;
Fixed                           gViewCosFrac;
Fixed                           gViewSinFrac;
float                           gViewCos;
float                           gViewSin;
float                           gNearPlaneW;
float                           gNearPlaneH;
float                           gNearPlaneHalfW;
float                           gNearPlaneHalfH;
float                           gNearPlaneP1x;
float                           gNearPlaneP1y;
float                           gNearPlaneP2x;
float                           gNearPlaneP2y;
float                           gNearPlaneTz;
float                           gNearPlaneBz;
float                           gNearPlaneXStepPerViewCol;
float                           gNearPlaneYStepPerViewCol;
float                           gNearPlaneZStepPerViewColPixel;
ProjectionMatrix                gProjMatrix;
uint32_t                        gExtraLight;
angle_t                         gClipAngleBAM;
angle_t                         gDoubleClipAngleBAM;
uint32_t                        gSprOpening[MAXSCREENWIDTH];
std::vector<SegClip>            gSegClip;
std::vector<OccludingColumns>   gOccludingCols;
uint32_t                        gNumFullSegCols;
std::vector<WallFragment>       gWallFragments;
std::vector<FlatFragment>       gFloorFragments;
std::vector<FlatFragment>       gCeilFragments;
std::vector<SkyFragment>        gSkyFragments;
std::vector<DrawSprite>         gDrawSprites;
std::vector<SpriteFragment>     gSpriteFragments;

//----------------------------------------------------------------------------------------------------------------------
// Load in the "TextureInfo" array so that the game knows all about the wall and sky textures (Width,Height).
// Also initialize the texture translation table for wall animations. 
// Called on startup only.
//----------------------------------------------------------------------------------------------------------------------
static void initData() noexcept {
    // Initialize render asset managers
    Textures::init();
    Sprites::init();
    
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

static void setupSegYClipArrayForDraw() noexcept {
    // Ensure the array is the correct size for the screen
    if (gSegClip.size() != gScreenWidth) {
        gSegClip.resize(gScreenWidth);
    }

    // Clear 8 entries in the y clip array at a time so the ops can be pipelined
    const SegClip segClipClearVal = SegClip{ (int16_t) -1, (int16_t) gScreenHeight };

    SegClip* pSegClip = gSegClip.data();
    SegClip* const pEndClipBounds8 = pSegClip + ((uint32_t(gSegClip.size()) / 8) * 8);
    SegClip* const pEndClipBounds = pSegClip + gSegClip.size();

    while (pSegClip < pEndClipBounds8) {
        pSegClip[0] = segClipClearVal;
        pSegClip[1] = segClipClearVal;
        pSegClip[2] = segClipClearVal;
        pSegClip[3] = segClipClearVal;
        pSegClip[4] = segClipClearVal;
        pSegClip[5] = segClipClearVal;
        pSegClip[6] = segClipClearVal;
        pSegClip[7] = segClipClearVal;
        pSegClip += 8;
    }

    // Clear any remaining entries
    while (pSegClip < pEndClipBounds) {
        pSegClip[0] = segClipClearVal;
        ++pSegClip;
    }

    // Reset: this gets incremented every time segs fully occupy a screen column
    gNumFullSegCols = 0;
}

static void setupOccludingColumnsArrayForDraw() noexcept {
    // Ensure the array is the correct size for the screen
    if (gOccludingCols.size() != gScreenWidth) {
        gOccludingCols.resize(gScreenWidth);
    }

    // Clear 8 entries in array at a time so the ops can be pipelined
    OccludingColumns* pOccludingCols = gOccludingCols.data();
    OccludingColumns* const pEndOccludingCols8 = pOccludingCols + ((uint32_t(gOccludingCols.size()) / 8) * 8);
    OccludingColumns* const pEndOccludingCols = pOccludingCols + gOccludingCols.size();

    while (pOccludingCols < pEndOccludingCols8) {
        pOccludingCols[0].count = 0;
        pOccludingCols[1].count = 0;
        pOccludingCols[2].count = 0;
        pOccludingCols[3].count = 0;
        pOccludingCols[4].count = 0;
        pOccludingCols[5].count = 0;
        pOccludingCols[6].count = 0;
        pOccludingCols[7].count = 0;
        pOccludingCols += 8;
    }

    // Clear any remaining entries
    while (pOccludingCols < pEndOccludingCols) {
        pOccludingCols[0].count = 0;
        ++pOccludingCols;
    }
}

static void preDrawSetup() noexcept {
    // Set the position and angle of the view from the player
    const player_t& player = gPlayers;
    const mobj_t& mapObj = *player.mo;

    gViewXFrac = mapObj.x;
    gViewYFrac = mapObj.y;
    gViewX = FMath::doomFixed16ToFloat<float>(mapObj.x);
    gViewY = FMath::doomFixed16ToFloat<float>(mapObj.y);
    gViewZFrac = player.viewz;
    gViewZ = FMath::doomFixed16ToFloat<float>(player.viewz);
    gViewAngleBAM = mapObj.angle;
    gViewAngle = FMath::doomAngleToRadians<float>(mapObj.angle);

    #if ENABLE_DEBUG_CAMERA_Z_MOVEMENT
    {
        gViewZFrac += FMath::floatToDoomFixed16(gDebugCameraZOffset);
        gViewZ += gDebugCameraZOffset;
    }
    #endif

    // Precompute sine and cosine of view angle
    {
        const uint32_t angleIdx = gViewAngleBAM >> ANGLETOFINESHIFT;    // Precalc angle index
        gViewSinFrac = gFineSine[angleIdx];                             // Get the base sine value
        gViewCosFrac = gFineCosine[angleIdx];                           // Get the base cosine value
        gViewSin = std::sin(-gViewAngle + FMath::ANGLE_90<float>);
        gViewCos = std::cos(-gViewAngle + FMath::ANGLE_90<float>);
    }

    // View vectors
    gViewDirX = std::cos(gViewAngle);
    gViewDirY = std::sin(gViewAngle);
    gViewPerpX = gViewDirY;
    gViewPerpY = -gViewDirX;

    // Near plane width and height
    gNearPlaneW = Z_NEAR * std::tan(FOV * 0.5f) * 2.0f;
    gNearPlaneH = gNearPlaneW / VIEW_ASPECT_RATIO;
    gNearPlaneHalfW = gNearPlaneW * 0.5f;
    gNearPlaneHalfH = gNearPlaneH * 0.5f;
    
    // Near plane left and right side x,y and top and bottom z
    gNearPlaneP1x = gViewX + gViewDirX * Z_NEAR - gNearPlaneHalfW * gViewPerpX;
    gNearPlaneP1y = gViewY + gViewDirY * Z_NEAR - gNearPlaneHalfW * gViewPerpY;
    gNearPlaneP2x = gViewX + gViewDirX * Z_NEAR + gNearPlaneHalfW * gViewPerpX;
    gNearPlaneP2y = gViewY + gViewDirY * Z_NEAR + gNearPlaneHalfW * gViewPerpY;
    gNearPlaneTz = gViewZ + gNearPlaneHalfH;
    gNearPlaneBz = gViewZ - gNearPlaneHalfH;

    // World X and Y step per column of screen pixels at the near plane
    gNearPlaneXStepPerViewCol = (gNearPlaneP2x - gNearPlaneP1x) / ((float) gScreenWidth);
    gNearPlaneYStepPerViewCol = (gNearPlaneP2y - gNearPlaneP1y) / ((float) gScreenWidth);
    gNearPlaneZStepPerViewColPixel = (gNearPlaneBz - gNearPlaneTz) / ((float) gScreenHeight);

    // Clear render arrays & buffers
    setupSegYClipArrayForDraw();
    setupOccludingColumnsArrayForDraw();

    gWallFragments.clear();
    gFloorFragments.clear();
    gCeilFragments.clear();
    gSkyFragments.clear();
    gDrawSprites.clear();
    gSpriteFragments.clear();

    gpEndVisPlane = gVisPlanes + 1;     // visplanes[0] is left empty
    gpEndVisWall = gVisWalls;           // No walls added yet
    gpEndVisSprite = gVisSprites;       // No sprites added yet
    gpEndOpening = gOpenings;           // No openings found

    // Other misc setup
    gExtraLight = player.extralight << 6;       // Init the extra lighting value
}

void init() noexcept {
    initData();                                 // Init resource managers and all of the lookup tables
    gClipAngleBAM = gXToViewAngle[0];           // Get the left clip angle from viewport
    gDoubleClipAngleBAM = gClipAngleBAM * 2;    // Precalc angle * 2

    // Fragment reserve
    gWallFragments.reserve(1024 * 8);
    gFloorFragments.reserve(1024 * 8);
    gCeilFragments.reserve(1024 * 8);
    gSkyFragments.reserve(1024);
    gDrawSprites.reserve(128);
    gSpriteFragments.reserve(1024 * 4);
}

void initMathTables() noexcept {
    gScreenWidth = SCREEN_WIDTHS[gScreenSize];
    gScreenHeight = SCREEN_HEIGHTS[gScreenSize];
    gCenterX = gScreenWidth / 2;
    gCenterY = gScreenHeight / 2;
    // TODO: REMOVE
    #if HACK_TEST_HIGH_RES_RENDERING
        gScreenXOffset = (320 * HACK_TEST_HIGH_RENDER_SCALE - gScreenWidth) / 2;
        gScreenYOffset = (160 * HACK_TEST_HIGH_RENDER_SCALE - gScreenHeight) / 2;
    #else
        gScreenXOffset = (320 - gScreenWidth) / 2;
        gScreenYOffset = (160 - gScreenHeight) / 2;
    #endif

    gGunXScale = (gScreenWidth * 0x100000) / 320;       // Get the 3DO scale factor for the gun shape and the y scale
    gGunYScale = (gScreenHeight * 0x10000) / 160;
    gStretch = STRETCHES[gScreenSize];
    gStretchWidth = gStretch * ((int) gScreenWidth / 2);

    // TODO: REMOVE
    #if !HACK_TEST_HIGH_RES_RENDERING
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
            float j = (float) i - (float) gScreenHeight * 0.5f + 0.5f;
            j = FMath::doomFixed16ToFloat<float>(gStretchWidth) / std::abs(j);
            j = std::fmin(j, 8192.0f);
            gYSlope[i] = j;
        }

        // Create the distance scale table for floor and ceiling textures 
        for (uint32_t i = 0; i < gScreenWidth; ++i) {
            const float c = std::cos(getViewAngleForX(i));
            gDistScale[i] = (1.0f / std::abs(c));
        }
    #endif  // #if !HACK_TEST_HIGH_RES_RENDERING

    // Create the lighting tables
    for (uint32_t i = 0; i < 256; ++i) {
        constexpr float LIGHT_MIN_PERCENT = 1.0f / 5.0f;
        constexpr float MAX_BRIGHT_RANGE_SCALE = 1.0f;
        constexpr float LIGHT_COEF_BASE = 16.0f;
        constexpr float LIGHT_COEF_ADJUST_FACTOR = 13.0f;
        
        const float lightLevel = (float) i / 255.0f;
        const float maxBrightRange = lightLevel * MAX_BRIGHT_RANGE_SCALE;

        gLightMins[i] = (float) i * LIGHT_MIN_PERCENT;
        gLightSubs[i] = maxBrightRange;
        gLightCoefs[i] = LIGHT_COEF_BASE - lightLevel * LIGHT_COEF_ADJUST_FACTOR;
    }

    // Compute the partial projection matrix
    {
        // This is largely based on GLM's 'perspectiveRH_ZO' - see definition of 'ProjectionMatrix'
        // for more details about these calculations:
        const float f = std::tan(FOV * 0.5f);
        const float a = VIEW_ASPECT_RATIO;

        gProjMatrix.r0c0 = 1.0f / f;
        gProjMatrix.r1c1 = -a / f;
        gProjMatrix.r2c2 = -Z_FAR / (Z_NEAR - Z_FAR);
        gProjMatrix.r2c3 = -(Z_NEAR * Z_FAR) / (Z_FAR - Z_NEAR);
    }
}

void drawPlayerView() noexcept {
    preDrawSetup();                 // Init variables based on camera angle
    doBspTraversal();               // Traverse the BSP tree and build lists of walls, floors (visplanes) and sprites to render

    // TODO: REMOVE
    #if false
    drawAllLineSegs();              // Draw all everything Z Sorted
    #endif
    
    drawAllSkyFragments();
    drawAllFloorFragments();
    drawAllCeilingFragments();
    drawAllWallFragments();
    drawAllVisPlanes();
    drawAllSprites();
    drawWeapons();                  // Draw the weapons on top of the screen
    doPostFx();                     // Draw color overlay if needed    
}

float LightParams::getLightMulForDist(const float dist) const noexcept {
    const float distFactorLinear = std::fmax(dist - lightSub, 0.0f);
    const float distFactorQuad = std::sqrtf(distFactorLinear);
    const float lightDiminish = distFactorQuad * lightCoef;
    
    float lightValue = 255.0f - lightDiminish;
    lightValue = std::fmax(lightValue, lightMin);
    lightValue = std::fmin(lightValue, lightMax);

    const float lightMul = lightValue * (1.0f / MAX_LIGHT_VALUE);
    return std::fmax(lightMul, MIN_LIGHT_MUL);
}

LightParams getLightParams(const uint32_t sectorLightLevel) noexcept {
    const uint32_t lightMax = std::min(sectorLightLevel, (uint32_t) C_ARRAY_SIZE(gLightCoefs) - 1);

    LightParams out;
    out.lightMin = gLightMins[lightMax];
    out.lightMax = (float) lightMax;
    out.lightSub = gLightSubs[lightMax];
    out.lightCoef = gLightCoefs[lightMax];

    return out;
}

END_NAMESPACE(Renderer)
