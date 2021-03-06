#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Blit.h"
#include "Game/Config.h"
#include "Game/Data.h"
#include "Sprites.h"
#include "Textures.h"
#include "Things/MapObj.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

// Sizes of the 3D view at the original 320x200 resolution
struct ScreenSize {
    uint16_t w;
    uint16_t h;
};

static constexpr ScreenSize REFERENCE_SCREEN_SIZES[6] = {
    { 280, 160 },
    { 256, 144 },
    { 224, 128 },
    { 192, 112 },
    { 160, 96 },
    { 128, 80 },
};

//------------------------------------------------------------------------------------------------------------------------------------------
// Internal renderer cross module globals
//------------------------------------------------------------------------------------------------------------------------------------------
float                           gDebugCameraZOffset;
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
std::vector<angle_t>            gScreenXToAngleBAM;
std::vector<DrawSeg>            gDrawSegs;
std::vector<SegClip>            gSegClip;
std::vector<OccludingColumns>   gOccludingCols;
uint32_t                        gNumFullSegCols;
std::vector<WallFragment>       gWallFragments;
std::vector<FlatFragment>       gFloorFragments;
std::vector<FlatFragment>       gCeilFragments;
std::vector<SkyFragment>        gSkyFragments;
std::vector<DrawSprite>         gDrawSprites;

//------------------------------------------------------------------------------------------------------------------------------------------
// Load in the "TextureInfo" array so that the game knows all about the wall and sky textures (Width,Height).
// Also initialize the texture translation table for wall animations.
// Called on startup only.
//------------------------------------------------------------------------------------------------------------------------------------------
static void initData() noexcept {
    // Initialize render asset managers
    Textures::init();
    Sprites::init();

    // Create a recipocal mul table so that I can divide 0-8191 from 1.0.
    // This way I can fake a divide with a multiply.
    gIDivTable[0] = UINT32_MAX;
    constexpr uint32_t NUM_DIV_TABLE_ENTRIES = C_ARRAY_SIZE(gIDivTable);

    for (uint32_t i = 1; i < NUM_DIV_TABLE_ENTRIES; ++i) {
        gIDivTable[i] = (uint32_t) fixed16Div(512 << FRACBITS, (int32_t) i << FRACBITS);  // 512.0 / i
    }

    // First time init of the math tables.
    // They may change however if the view size changes!
    initMathTables();
}

static void setupSegYClipArrayForDraw() noexcept {
    // Ensure the array is the correct size for the screen
    if (gSegClip.size() != g3dViewWidth) {
        gSegClip.resize(g3dViewWidth);
    }

    // Clear 8 entries in the y clip array at a time so the ops can be pipelined
    const SegClip segClipClearVal = SegClip{ (int16_t) -1, (int16_t) g3dViewHeight };

    SegClip* pSegClip = gSegClip.data();
    SegClip* const pEndClipBounds8 = pSegClip + (uintptr_t)(g3dViewWidth / 8) * 8;
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
    if (gOccludingCols.size() != g3dViewWidth) {
        gOccludingCols.resize(g3dViewWidth);
    }

    // Clear 8 entries in array at a time so the ops can be pipelined
    OccludingColumns* pOccludingCols = gOccludingCols.data();
    OccludingColumns* const pEndOccludingCols8 = pOccludingCols + (uintptr_t)(g3dViewWidth / 8) * 8;
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
    const player_t& player = gPlayer;
    const mobj_t& mapObj = *player.mo;

    gViewXFrac = mapObj.x;
    gViewYFrac = mapObj.y;
    gViewX = fixed16ToFloat(mapObj.x);
    gViewY = fixed16ToFloat(mapObj.y);
    gViewZFrac = player.viewz;
    gViewZ = fixed16ToFloat(player.viewz);
    gViewAngleBAM = mapObj.angle;
    gViewAngle = bamAngleToRadians(mapObj.angle);

    if (Config::gbAllowDebugCameraUpDownMovement) {
        gViewZFrac += floatToFixed16(gDebugCameraZOffset);
        gViewZ += gDebugCameraZOffset;
    }

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

    // Near plane left and right side x,y and top and bottom z
    gNearPlaneP1x = gViewX + gViewDirX * Z_NEAR - gNearPlaneHalfW * gViewPerpX;
    gNearPlaneP1y = gViewY + gViewDirY * Z_NEAR - gNearPlaneHalfW * gViewPerpY;
    gNearPlaneP2x = gViewX + gViewDirX * Z_NEAR + gNearPlaneHalfW * gViewPerpX;
    gNearPlaneP2y = gViewY + gViewDirY * Z_NEAR + gNearPlaneHalfW * gViewPerpY;
    gNearPlaneTz = gViewZ + gNearPlaneHalfH;
    gNearPlaneBz = gViewZ - gNearPlaneHalfH;

    // World X and Y step per column of screen pixels at the near plane
    gNearPlaneXStepPerViewCol = (gNearPlaneP2x - gNearPlaneP1x) / ((float) g3dViewWidth);
    gNearPlaneYStepPerViewCol = (gNearPlaneP2y - gNearPlaneP1y) / ((float) g3dViewWidth);
    gNearPlaneZStepPerViewColPixel = (gNearPlaneBz - gNearPlaneTz) / ((float) g3dViewHeight);

    // Clear render arrays & buffers
    setupSegYClipArrayForDraw();
    setupOccludingColumnsArrayForDraw();

    gWallFragments.clear();
    gFloorFragments.clear();
    gCeilFragments.clear();
    gSkyFragments.clear();
    gDrawSprites.clear();

    // Other misc setup
    gExtraLight = player.extralight << 6;       // Init the extra lighting value
}

void init() noexcept {
    initData();     // Init resource managers and all of the lookup tables

    // Fragment reserve
    gWallFragments.reserve(1024 * 8);
    gFloorFragments.reserve(1024 * 8);
    gCeilFragments.reserve(1024 * 8);
    gSkyFragments.reserve(1024);
    gDrawSprites.reserve(128);
}

void shutdown() noexcept {
    // Nothing to do here yet...
}

void initMathTables() noexcept {
    // Compute stuff based on screen size
    gScaleFactor = (float) Video::gScreenWidth / (float) Video::REFERENCE_SCREEN_WIDTH;
    gInvScaleFactor = 1.0f / gScaleFactor;
    g3dViewWidth = (uint32_t)((float) REFERENCE_SCREEN_SIZES[gScreenSize].w * gScaleFactor);
    g3dViewHeight = (uint32_t)((float) REFERENCE_SCREEN_SIZES[gScreenSize].h * gScaleFactor);
    gCenterX = g3dViewWidth / 2;
    gCenterY = g3dViewHeight / 2;

    #if ASSERTS_ENABLED == 1
        // Sanity check the offset calculation will never underflow!
        for (uint32_t i = 0; i < C_ARRAY_SIZE(REFERENCE_SCREEN_SIZES); ++i) {
            ASSERT(REFERENCE_SCREEN_SIZES[i].w <= 320);
            ASSERT(REFERENCE_SCREEN_SIZES[i].h <= 160);
        }
    #endif

    const uint32_t refScreenOffsetX = (320u - REFERENCE_SCREEN_SIZES[gScreenSize].w) / 2u;
    const uint32_t refScreenOffsetY = (160u - REFERENCE_SCREEN_SIZES[gScreenSize].h) / 2u;
    g3dViewXOffset = (uint32_t)((float) refScreenOffsetX * gScaleFactor);
    g3dViewYOffset = (uint32_t)((float) refScreenOffsetY * gScaleFactor);

    gGunXScale = (float) g3dViewWidth / 320.0f;     // Get the 3DO scale factor for the gun shape and the y scale
    gGunYScale = (float) g3dViewHeight / 160.0f;

    // Near plane width and height
    gNearPlaneW = Z_NEAR * std::tan(FOV * 0.5f) * 2.0f;
    gNearPlaneH = gNearPlaneW / VIEW_ASPECT_RATIO;
    gNearPlaneHalfW = gNearPlaneW * 0.5f;
    gNearPlaneHalfH = gNearPlaneH * 0.5f;

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

    // Compute the screen pixel to view angle table
    gScreenXToAngleBAM.resize(g3dViewWidth);

    {
        float screenXToT = 1.0f / ((float) g3dViewWidth - 1.0f);

        for (uint32_t x = 0; x < g3dViewWidth; ++x) {
            const float t = ((float) x + 0.5f) * screenXToT;
            const float nearPlaneX = t * gNearPlaneW - gNearPlaneHalfW;
            const float angleRad = std::atan2(Z_NEAR, nearPlaneX);
            const angle_t angleBAM = radiansToBamAngle(angleRad);
            gScreenXToAngleBAM[x] = angleBAM;
        }
    }

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
}

void drawPlayerView() noexcept {
    preDrawSetup();                 // Init variables based on camera angle
    doBspTraversal();               // Traverse the BSP tree and build lists of walls, floors (visplanes) and sprites to render
    drawAllSkyFragments();
    drawAllFloorFragments();
    drawAllCeilingFragments();
    drawAllWallFragments();
    drawAllSprites();
    drawWeapons();                  // Draw the weapons on top of the screen
    doPostFx();                     // Draw color overlay if needed
}

float LightParams::getLightMulForDist(const float dist) const noexcept {
    const float distFactorLinear = std::max(dist - lightSub, 0.0f);
    const float distFactorQuad = std::sqrt(distFactorLinear);
    const float lightDiminish = distFactorQuad * lightCoef;

    float lightValue = 255.0f - lightDiminish;
    lightValue = std::max(lightValue, lightMin);
    lightValue = std::min(lightValue, lightMax);

    const float lightMul = lightValue * (1.0f / MAX_LIGHT_VALUE);
    return std::max(lightMul, MIN_LIGHT_MUL);
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
