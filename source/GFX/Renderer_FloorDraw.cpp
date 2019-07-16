#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/FMath.h"
#include "Base/Tables.h"
#include "Blit.h"
#include "Game/Data.h"
#include "Textures.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

static uint32_t gSpanStart[MAXSCREENHEIGHT];

//----------------------------------------------------------------------------------------------------------------------
// Draws a horizontal span of the floor
//----------------------------------------------------------------------------------------------------------------------
static void drawFloorColumn(
    const uint32_t colY,
    const uint32_t colX,
    const uint32_t numPixels,
    const float texXFrac,
    const float texYFrac,
    const float texXStep,
    const float texYStep,
    const ImageData& texData
) noexcept {
    const Fixed lightMultiplier = getLightMultiplier(gTxTextureLight, MAX_FLOOR_LIGHT_VALUE);

    Blit::blitColumn<
        Blit::BCF_HORZ_COLUMN |     // Column is horizontal rather than vertical
        Blit::BCF_ROW_MAJOR_IMG |   // Floor textures are stored in row major format (unlike sprites and walls)
        Blit::BCF_STEP_X |
        Blit::BCF_STEP_Y |
        Blit::BCF_H_WRAP_64 |       // Floor textures are always 64x64!
        Blit::BCF_V_WRAP_64 |
        Blit::BCF_COLOR_MULT_RGB
    >(
        texData,
        texXFrac,
        texYFrac,
        Video::gFrameBuffer,
        Video::SCREEN_WIDTH,
        Video::SCREEN_HEIGHT,
        colX + gScreenXOffset,
        colY + gScreenYOffset,
        numPixels,
        texXStep,
        texYStep,
        lightMultiplier,
        lightMultiplier,
        lightMultiplier
    );
}

//----------------------------------------------------------------------------------------------------------------------
// This is the basic function to draw horizontal lines quickly 
//----------------------------------------------------------------------------------------------------------------------
static void mapPlane(
    const uint32_t x2,
    const uint32_t y,
    const float planeY,
    const float planeHeight,
    const float baseXScale,
    const float baseYScale,
    const ImageData& texData
) noexcept {
    //------------------------------------------------------------------------------------------------------------------
    // planeheight is 10.6
    // yslope is 6.10, distscale is 1.15
    // distance is 12.4
    // length is 11.5
    //------------------------------------------------------------------------------------------------------------------
    const uint32_t x1 = gSpanStart[y];
    const float distance = gYSlope[y] * planeHeight;    // Get the offset for the plane height
    const float length = gDistScale[x1] * distance;
    const float angle = -getViewAngleForX(x1) + FMath::doomAngleToRadians<float>(gViewAngle);

    // xfrac, yfrac, xstep, ystep
    const float xfrac = std::cos(angle) * length + FMath::doomFixed16ToFloat<float>(gViewX);
    const float yfrac = planeY - std::sin(angle) * length;
    const float xstep = distance * baseXScale;
    const float ystep = distance * baseYScale;

    {
        // Light params
        const float lightCoef = FMath::doomFixed16ToFloat<float>(gLightCoef << 9);
        const float lightMin = (float) gLightMin;
        const float lightMax = (float) gLightMax;
        const float lightSub = (float) gLightSub;

        const float distCoef = 1.0f / (distance * (1.0f / 16.0f));
        float lightValue = lightCoef * distCoef - gLightSub;
        lightValue = std::max(lightValue, lightMin);
        lightValue = std::min(lightValue, lightMax);

        gTxTextureLight = (uint32_t) lightValue;
    }

    drawFloorColumn(y, x1, x2 - x1, xfrac, yfrac, xstep, ystep, texData);
}

//----------------------------------------------------------------------------------------------------------------------
// Draw a plane by scanning the open records. 
// The open records are an array of top and bottom Y's for a graphical plane.
// I traverse the array to find out the horizontal spans I need to draw; this is a bottleneck routine.
//----------------------------------------------------------------------------------------------------------------------
void drawVisPlane(
    visplane_t& plane,
    const float planeY,
    const float baseXScale,
    const float baseYScale
) noexcept {
    const Texture* const pTex = getFlatAnimTexture((uint32_t) plane.picHandle);
    const ImageData& texData = pTex->data;
    const float planeHeight = std::abs(plane.height);
    
    {
        const uint32_t lightLevel = plane.planeLight;
        gLightMin = gLightMins[lightLevel];
        gLightMax = lightLevel;
        gLightSub = gLightSubs[lightLevel];
        gLightCoef = gPlaneLightCoef[lightLevel];
    }
    
    uint32_t stop = plane.maxX + 1;         // Maximum x coord
    uint32_t x = plane.minX;                // Starting x

    ColumnYBounds prevCol = { MAXSCREENHEIGHT - 1, 0 };     // Set posts to stop drawing
    ColumnYBounds* const pPlaneCols = plane.cols;           // Init the pointer to the open Y's
    pPlaneCols[stop] = prevCol;
    
    do {
        const ColumnYBounds newCol = pPlaneCols[x];     // Fetch the NEW top and bottom
        
        if (prevCol == newCol)
            continue;
        
        uint32_t prevTopY = prevCol.topY;        // Previous and dest Y coords for top and bottom line
        uint32_t prevBottomY = prevCol.bottomY;
        uint32_t newTopY = newCol.topY;
        uint32_t newBottomY = newCol.bottomY;
        
        // For lines on the top, check if the entry is going down            
        if (prevTopY < newTopY && prevTopY <= prevBottomY) {        // Valid?
            uint32_t count = prevBottomY + 1;                       // Convert to <
            if (newTopY < count) {                                  // Use the lower
                count = newTopY;                                    // This is smaller
            }

            do {
                // Draw to this x
                mapPlane(
                    x,
                    prevTopY,
                    planeY,
                    planeHeight,
                    baseXScale,
                    baseYScale,
                    texData
                );
            } while (++prevTopY < count);   // Keep counting
        }

        if (newTopY < prevTopY && newTopY <= newBottomY) {
            uint32_t count = newBottomY + 1;
            if (prevTopY < count) {
                count = prevTopY;
            }

            do {
                gSpanStart[newTopY] = x;    // Mark the starting x's
            } while (++newTopY < count);
        }
            
        if (prevBottomY > newBottomY && prevBottomY >= prevTopY) {
            int32_t count = prevTopY - 1;
            if (count < (int32_t) newBottomY) {
                count = newBottomY;
            }

            do {
                // Draw to this x
                mapPlane(
                    x,
                    prevBottomY,
                    planeY,
                    planeHeight,
                    baseXScale,
                    baseYScale,
                    texData
                );
            } while ((int32_t) --prevBottomY > count);
        }

        if (newBottomY > prevBottomY && newBottomY >= newTopY) {
            int count = newTopY - 1;
            if (count < (int32_t) prevBottomY) {
                count = prevBottomY;
            }

            do {
                gSpanStart[newBottomY] = x;     // Mark the starting x's
            } while ((int32_t) --newBottomY > count);
        }

        prevCol = newCol;
    } while (++x <= stop);
}

void drawAllVisPlanes() noexcept {
    const float viewAngle = FMath::doomAngleToRadians<float>(gViewAngle);
    const float wallScaleAngle = viewAngle - FMath::ANGLE_90<float>;    // Left to right mapping

    const float baseXScale = +std::cos(wallScaleAngle) / ((float) gScreenWidth * 0.5f);
    const float baseYScale = -std::sin(wallScaleAngle) / ((float) gScreenWidth * 0.5f);
    const float planeY = FMath::doomFixed16ToFloat<float>(-gViewY);     // Get the Y coord for camera

    visplane_t* pPlane = gVisPlanes + 1;
    visplane_t* const pEndPlane = gpEndVisPlane;
    
    while (pPlane < pEndPlane) {
        drawVisPlane(*pPlane, planeY, baseXScale, baseYScale);
        ++pPlane;
    }
}

END_NAMESPACE(Renderer)
