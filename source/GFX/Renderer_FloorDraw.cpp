#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Textures.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

static uint32_t gSpanStart[MAXSCREENHEIGHT];

//----------------------------------------------------------------------------------------------------------------------
// Draws a horizontal span of the floor.
//----------------------------------------------------------------------------------------------------------------------
static void drawFloorColumn(
    const uint32_t ds_y,
    const uint32_t ds_x1,
    const uint32_t Count,
    const uint32_t xfrac,
    const uint32_t yfrac,
    const Fixed ds_xstep,
    const Fixed ds_ystep,
    const std::byte* const pTexData
) noexcept {
    // FIXME: TEMP - CLEANUP & OPTIMIZE
    const uint16_t* const pPLUT = (const uint16_t*) pTexData;
    const Fixed lightMultiplier = getLightMultiplier(gTxTextureLight, MAX_FLOOR_LIGHT_VALUE);

    for (uint32_t pixelNum = 0; pixelNum < Count; ++pixelNum) {
        Fixed tx = ((xfrac + ds_xstep * pixelNum) >> FRACBITS) & 63;    // assumes 64x64
        Fixed ty = ((yfrac + ds_ystep * pixelNum) >> FRACBITS) & 63;    // assumes 64x64

        Fixed offset = ty * 64 + tx;
        const uint8_t lutByte = ((uint8_t) pTexData[64 + offset]) & 31;
        ASSERT(lutByte < 32);
        uint8_t colorIdx = lutByte;

        const uint16_t color = byteSwappedU16(pPLUT[colorIdx]);
        const uint16_t texR = (color & 0b0111110000000000) >> 10;
        const uint16_t texG = (color & 0b0000001111100000) >> 5;
        const uint16_t texB = (color & 0b0000000000011111) >> 0;

        const Fixed texRFrac = intToFixed(texR);
        const Fixed texGFrac = intToFixed(texG);
        const Fixed texBFrac = intToFixed(texB);

        const Fixed darkenedR = fixedMul(texRFrac, lightMultiplier);
        const Fixed darkenedG = fixedMul(texGFrac, lightMultiplier);
        const Fixed darkenedB = fixedMul(texBFrac, lightMultiplier);

        const uint32_t finalColor = Video::fixedRgbToScreenCol(darkenedR, darkenedG, darkenedB);        
        const uint32_t screenX = ds_x1 + pixelNum + gScreenXOffset;
        const uint32_t screenY = ds_y + gScreenYOffset;

        Video::gFrameBuffer[screenY * Video::SCREEN_WIDTH + screenX] = finalColor;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// This is the basic function to draw horizontal lines quickly 
//----------------------------------------------------------------------------------------------------------------------
static void mapPlane(
    const uint32_t x2,
    const uint32_t y,
    const Fixed planeY,
    const Fixed planeHeight,
    const Fixed baseXScale,
    const Fixed baseYScale,
    const std::byte* const pTexData
) noexcept {
    //------------------------------------------------------------------------------------------------------------------
    // planeheight is 10.6
    // yslope is 6.10, distscale is 1.15
    // distance is 12.4
    // length is 11.5
    //------------------------------------------------------------------------------------------------------------------
    const uint32_t x1 = gSpanStart[y];
    const uint32_t distance = (gYSlope[y] * planeHeight) >> 12;     // Get the offset for the plane height
    const Fixed length = (gDistScale[x1] * distance) >> 14;
    const angle_t angle = (gXToViewAngle[x1] + gViewAngle) >> ANGLETOFINESHIFT;

    // xfrac, yfrac, xstep, ystep
    const Fixed xfrac = (((gFineCosine[angle] >> 1) * length) >> 4) + gViewX;
    const Fixed yfrac = planeY - (((gFineSine[angle] >> 1) * length) >> 4);

    const Fixed xstep = ((Fixed) distance * baseXScale) >> 4;
    const Fixed ystep = ((Fixed) distance * baseYScale) >> 4;

    {
        const uint32_t distanceDiv = (distance > 0) ? distance : 1;         // DC: fix division by zero when using the noclip cheat
        Fixed lightValue = gLightCoef / (Fixed) distanceDiv - gLightSub;

        if (lightValue < gLightMin) {
            lightValue = gLightMin;
        }
    
        if (lightValue > gLightMax) {
            lightValue = gLightMax;
        }

        gTxTextureLight = (uint32_t) lightValue;
    }

    drawFloorColumn(y, x1, x2 - x1, xfrac, yfrac, xstep, ystep, pTexData);
}

//----------------------------------------------------------------------------------------------------------------------
// Draw a plane by scanning the open records. 
// The open records are an array of top and bottom Y's for a graphical plane.
// I traverse the array to find out the horizontal spans I need to draw; this is a bottleneck routine.
//----------------------------------------------------------------------------------------------------------------------
void drawVisPlane(
    visplane_t& plane,
    const Fixed planeY,
    const Fixed baseXScale,
    const Fixed baseYScale
) noexcept {
    const Texture* const pTex = getFlatAnimTexture((uint32_t) plane.picHandle);
    const std::byte* const pTexData = (std::byte*) pTex->pData;
    const Fixed planeHeight = std::abs(plane.height);
    
    {
        const uint32_t lightLevel = plane.planeLight;
        gLightMin = gLightMins[lightLevel];
        gLightMax = lightLevel;
        gLightSub = gLightSubs[lightLevel];
        gLightCoef = gPlaneLightCoef[lightLevel];
    }
    
    uint32_t stop = plane.maxX + 1;         // Maximum x coord
    uint32_t x = plane.minX;                // Starting x

    ColumnYBounds* const pPlaneCols = plane.cols;           // Init the pointer to the open Y's
    
    ColumnYBounds prevCol = { MAXSCREENHEIGHT - 1, 0 };     // Set posts to stop drawing
    pPlaneCols[stop] = prevCol;

    do {
        const ColumnYBounds newCol = pPlaneCols[x];     // Fetch the NEW top and bottom
        const bool bSameColBounds = (
            (prevCol.topY == newCol.topY) && 
            (prevCol.bottomY == newCol.bottomY)
        );

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
                    pTexData
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
            int count = prevTopY - 1;
            if (count < (int) newBottomY) {
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
                    pTexData
                );
            } while ((int) --prevBottomY > count);
        }

        if (newBottomY > prevBottomY && newBottomY >= newTopY) {
            int count = newTopY - 1;
            if (count < (int) prevBottomY) {
                count = prevBottomY;
            }

            do {
                gSpanStart[newBottomY] = x; // Mark the starting x's
            } while ((int) --newBottomY > count);
        }

        prevCol = newCol;
    } while (++x <= stop);
}

void drawAllVisPlanes() noexcept {
    const uint32_t wallScale = (gViewAngle - ANG90) >> ANGLETOFINESHIFT;                    // Left to right mapping
    const Fixed baseXScale = (gFineCosine[wallScale] / ((int32_t) gScreenWidth / 2));
    const Fixed baseYScale = -(gFineSine[wallScale] / ((int32_t) gScreenWidth / 2));
    const Fixed planeY = -gViewY;                                                           // Get the Y coord for camera

    visplane_t* pPlane = gVisPlanes + 1;
    visplane_t* const pEndPlane = gpEndVisPlane;
    
    while (pPlane < pEndPlane) {
        drawVisPlane(*pPlane, planeY, baseXScale, baseYScale);
        ++pPlane;
    }
}

END_NAMESPACE(Renderer)
