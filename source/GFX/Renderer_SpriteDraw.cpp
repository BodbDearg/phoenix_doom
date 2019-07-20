#include "Renderer_Internal.h"

#include "Base/FMath.h"
#include "Base/Tables.h"
#include "Blit.h"
#include "Map/MapData.h"
#include "Sprites.h"
#include "Things/MapObj.h"
#include "Video.h"
#include <algorithm>

BEGIN_NAMESPACE(Renderer)

//----------------------------------------------------------------------------------------------------------------------
// Get the pointer to the first pixel in a particular column of a sprite.
// The input x coordinate is in 16.16 fixed point format and the output is clamped to sprite bounds.
//----------------------------------------------------------------------------------------------------------------------
static const uint16_t* getSpriteColumn(const vissprite_t& visSprite, const Fixed xFrac) {
    const int32_t x = fixedToInt(xFrac);
    const SpriteFrameAngle* const pSprite = visSprite.pSprite;
    const int32_t xClamped = (x < 0) ? 0 : ((x >= pSprite->width) ? pSprite->width - 1 : x);
    return &pSprite->pTexture[pSprite->height * xClamped];
}

//----------------------------------------------------------------------------------------------------------------------
// This routine will draw a scaled sprite during the game. It is called when there is no 
// onscreen clipping needed or if the only clipping is to the screen bounds.
//----------------------------------------------------------------------------------------------------------------------
static void drawSpriteNoClip(const vissprite_t& visSprite) noexcept {
    // Get the left, right, top and bottom screen edges for the sprite to be rendered.
    // Also check if the sprite is completely offscreen, because the input is not clipped.
    // 3DO Doom originally relied on the hardware to clip in this routine...
    int32_t x1 = visSprite.x1;
    int32_t x2 = visSprite.x2;
    int32_t y1 = visSprite.y1;
    int32_t y2 = visSprite.y2;

    const bool bCompletelyOffscreen = (
        (x1 >= (int32_t) gScreenWidth) ||
        (x2 < 0) ||
        (y1 >= (int32_t) gScreenHeight) ||
        (y2 < 0)
    );

    if (bCompletelyOffscreen)
        return;

    // Get the light multiplier to use for lighting the sprite
    const LightParams lightParams = getLightParams((visSprite.colormap & 0xFFu) + gExtraLight);
    const float viewStretchWidthF = FMath::doomFixed16ToFloat<float>(gStretchWidth);
    const float spriteScale = FMath::doomFixed16ToFloat<float>(visSprite.yscale);
    const float spriteDist = (1.0f / spriteScale) * viewStretchWidthF;
    const float lightMulF = lightParams.getLightMulForDist(spriteDist);
    const Fixed lightMulFrac = FMath::floatToDoomFixed16(lightMulF);
    
    // Get the width and height of the sprite and also the size that it will be rendered at.
    // Note that we expect no zero sizes here!
    const SpriteFrameAngle* const pSpriteFrame = visSprite.pSprite;
    
    const int32_t spriteW = pSpriteFrame->width;
    const int32_t spriteH = pSpriteFrame->height;
    ASSERT(spriteW > 0 && spriteH > 0);

    const uint32_t renderW = (visSprite.x2 - visSprite.x1) + 1;
    const uint32_t renderH = (visSprite.y2 - visSprite.y1) + 1;
    ASSERT(renderW > 0 && renderH > 0);

    // Figure out the step in texels we want per x and y pixel in 16.16 format
    const Fixed texelStepX_NoFlip = Blit::calcTexelStep(spriteW, renderW);
    const Fixed texelStepY = Blit::calcTexelStep(spriteH, renderH);

    // Computing start texel x coord (y is '0' for now) and step due to sprite flipping
    Fixed texelStepX;
    Fixed startTexelX;
    Fixed startTexelY = 0;

    if (pSpriteFrame->flipped != 0) {
        texelStepX = -texelStepX_NoFlip;
        startTexelX = intToFixed(spriteW) - 1;      // Start from the furthest reaches of the end pixel, only want texel to change if nearly 1 unit has been passed!
    }
    else {
        texelStepX = texelStepX_NoFlip;
        startTexelX = 0;
    }

    // Sanity check in debug that we won't go out of bounds of the texture (shouldn't)
    #if ASSERTS_ENABLED == 1
    {
        const Fixed endXFrac = startTexelX + fixedMul(texelStepX, intToFixed(renderW - 1));
        const Fixed endYFrac = fixedMul(texelStepY, intToFixed(renderH - 1));
        const int32_t endX = fixedToInt(endXFrac);
        const int32_t endY = fixedToInt(endYFrac);

        ASSERT(endX >= 0 && endX < spriteW);
        ASSERT(endY >= 0 && endY < spriteH);
    }
    #endif

    // Clip the sprite render bounds to the screen (left, right, top, bottom, in that order).
    // Skip over rows and columns that are out of bounds:
    if (x1 < 0) {
        const int32_t pixelsOffscreen = -x1;
        startTexelX += pixelsOffscreen * texelStepX;
        x1 = 0;
    }

    if (x2 >= (int32_t) gScreenWidth) {
        x2 = gScreenWidth - 1;
    }
    
    if (y1 < 0) {
        const int32_t pixelsOffscreen = -y1;
        startTexelY += pixelsOffscreen * texelStepY;
        y1 = 0;
    }

    if (y2 >= (int32_t) gScreenHeight) {
        y2 = gScreenHeight - 1;
    }
    
    // Render all the columns of the sprite
    const uint16_t* const pImage = pSpriteFrame->pTexture;
    Fixed texelXFrac = startTexelX;

    for (int32_t x = x1; x <= x2; ++x) {
        const int32_t texelXInt = fixedToInt(texelXFrac);
        Fixed texelYFrac = startTexelY;
        
        const uint16_t* const pImageCol = pImage + texelXInt * spriteH;
        uint32_t* pDstPixel = &Video::gFrameBuffer[x + gScreenXOffset + (y1 + gScreenYOffset) * Video::SCREEN_WIDTH];

        for (int32_t y = y1; y <= y2; ++y) {
            // Grab this pixels color from the sprite image and skip if alpha 0
            const int32_t texelYInt = fixedToInt(texelYFrac);

            const uint16_t color = pImageCol[texelYInt];
            const uint16_t texA = (color & 0b1000000000000000) >> 15;
            const uint16_t texR = (color & 0b0111110000000000) >> 10;
            const uint16_t texG = (color & 0b0000001111100000) >> 5;
            const uint16_t texB = (color & 0b0000000000011111) >> 0;

            if (texA != 0) {
                // Do light diminishing
                const Fixed texRFrac = intToFixed(texR);
                const Fixed texGFrac = intToFixed(texG);
                const Fixed texBFrac = intToFixed(texB);

                const Fixed darkenedR = fixedMul(texRFrac, lightMulFrac);
                const Fixed darkenedG = fixedMul(texGFrac, lightMulFrac);
                const Fixed darkenedB = fixedMul(texBFrac, lightMulFrac);

                // Save the final output color
                const uint32_t finalColor = Video::fixedRgbToScreenCol(darkenedR, darkenedG, darkenedB);
                *pDstPixel = finalColor;
            }

            // Onto the next pixel in the column
            texelYFrac += texelStepY;
            pDstPixel += Video::SCREEN_WIDTH;
        }

        texelXFrac += texelStepX;   // Next column
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Draws a column of a sprite that is clipped to the given top and bottom bounds
//----------------------------------------------------------------------------------------------------------------------
static void oneSpriteLine(
    const uint32_t screenX,
    const Fixed spriteX,
    const uint32_t topClipY,
    const uint32_t bottomClipY,
    const vissprite_t& visSprite,
    const Fixed lightMulFrac
) noexcept {
    // Sanity checks
    ASSERT(topClipY >= 0 && topClipY <= gScreenHeight);
    ASSERT(bottomClipY >= 0 && bottomClipY <= gScreenHeight);

    // Get the top and bottom screen edges for the sprite columnn to be rendered.
    // Also check if the column is completely offscreen, because the input is not clipped.
    // 3DO Doom originally relied on the hardware to clip in this routine...
    if (screenX < 0 || screenX >= (int32_t) gScreenWidth)
        return;
    
    int32_t y1 = visSprite.y1;
    int32_t y2 = visSprite.y2;
    
    if (y1 >= (int32_t) gScreenHeight || y2 < 0)
        return;
    
    // If the clip bounds are meeting (or past each other?!) then ignore
    if (topClipY >= bottomClipY)
        return;
    
    // Get the height of the sprite and also the height that it will be rendered at.
    // Note that we expect no zero sizes here!
    const SpriteFrameAngle* const pSpriteFrame = visSprite.pSprite;
    
    const int32_t spriteH = pSpriteFrame->height;
    const uint32_t renderH = (visSprite.y2 - visSprite.y1) + 1;
    ASSERT(spriteH > 0);
    ASSERT(renderH > 0);
    
    // Figure out the step in texels we want per y pixel in 16.16 format
    const Fixed texelStepY = Blit::calcTexelStep(spriteH, renderH);

    // Sanity check in debug that we won't go out of bounds of the texture (shouldn't)
    #if ASSERTS_ENABLED == 1
    {
        const Fixed endYFrac = fixedMul(texelStepY, intToFixed(renderH - 1));
        const int32_t endY = fixedToInt(endYFrac);
        ASSERT(endY >= 0 && endY < spriteH);
    }
    #endif
    
    // Clip the sprite render bounds to the screen (top, bottom, in that order).
    // Skip over rows that are out of bounds:
    Fixed startTexelY = 0;
    
    if (y1 < (int32_t) topClipY) {
        const int pixelsOffscreen = topClipY - y1;
        startTexelY += pixelsOffscreen * texelStepY;
        y1 = topClipY;
    }
    
    if (y2 >= (int32_t) bottomClipY) {
        y2 = bottomClipY - 1;
    }
    
    // Render the sprite column
    const uint16_t* const pImageCol = getSpriteColumn(visSprite, spriteX);
    Fixed texelYFrac = startTexelY;
    uint32_t* pDstPixel = &Video::gFrameBuffer[screenX + gScreenXOffset + (y1 + gScreenYOffset) * Video::SCREEN_WIDTH];

    for (int y = y1; y <= y2; ++y) {
        // Grab this pixels color from the sprite image and skip if alpha 0
        const int32_t texelYInt = fixedToInt(texelYFrac);
        
        const uint16_t color = pImageCol[texelYInt];
        const uint16_t texA = (color & 0b1000000000000000) >> 15;
        const uint16_t texR = (color & 0b0111110000000000) >> 10;
        const uint16_t texG = (color & 0b0000001111100000) >> 5;
        const uint16_t texB = (color & 0b0000000000011111) >> 0;

        if (texA != 0) {
            // Do light diminishing
            const Fixed texRFrac = intToFixed(texR);
            const Fixed texGFrac = intToFixed(texG);
            const Fixed texBFrac = intToFixed(texB);

            const Fixed darkenedR = fixedMul(texRFrac, lightMulFrac);
            const Fixed darkenedG = fixedMul(texGFrac, lightMulFrac);
            const Fixed darkenedB = fixedMul(texBFrac, lightMulFrac);
            
            // Save the final output color
            const uint32_t finalColor = Video::fixedRgbToScreenCol(darkenedR, darkenedG, darkenedB);
            *pDstPixel = finalColor;
        }
        
        // Onto the next pixel in the column
        texelYFrac += texelStepY;
        pDstPixel += Video::SCREEN_WIDTH;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Draws a clipped sprite to the screen
//----------------------------------------------------------------------------------------------------------------------
static void drawSpriteClip(const uint32_t x1, const uint32_t x2, const vissprite_t& visSprite) noexcept {
    // Get the light multiplier to use for lighting the sprite
    const LightParams lightParams = getLightParams((visSprite.colormap & 0xFFu) + gExtraLight);
    const float viewStretchWidthF = FMath::doomFixed16ToFloat<float>(gStretchWidth);
    const float spriteScale = FMath::doomFixed16ToFloat<float>(visSprite.yscale);
    const float spriteDist = (1.0f / spriteScale) * viewStretchWidthF;
    const float lightMulF = lightParams.getLightMulForDist(spriteDist);
    const Fixed lightMulFrac = FMath::floatToDoomFixed16(lightMulF);

    const Fixed spriteWidthFrac = intToFixed(visSprite.pSprite->width);
    
    int32_t y = visSprite.y1;
    int32_t y2 = visSprite.y2;
    const int32_t maxRun = y2 - y;
    
    if (y < 0) {
        y = 0;
    }
    
    if (y2 >= (int32_t) gScreenHeight) {
        y2 = (int32_t) gScreenHeight;
    }
    
    Fixed xFrac = 0;
    Fixed xStep = 0xFFFFFFFFUL / (uint32_t) visSprite.xscale;   // Get the recipocal for the X scale
    
    if (visSprite.colormap & 0x4000) {  // Step in the opposite direction?
        xStep = -xStep;
        xFrac = spriteWidthFrac - 1;
    }
    
    if (visSprite.x1 != x1) {   // How far should I skip?
        xFrac += xStep * (x1 - visSprite.x1);
    }
    
    uint32_t x = x1;
    
    do {
        uint32_t top = gSprOpening[x];  // Get the opening to the screen
        
        if (top == gScreenHeight) {  // Not clipped?
            oneSpriteLine(x, xFrac, 0, gScreenHeight, visSprite, lightMulFrac);
        } else {
            uint32_t bottom = top & 0xff;
            top >>= 8;
            
            if (top < bottom) {     // Valid run?
                if (y >= (int32_t) top && y2 < (int32_t) bottom) {
                    oneSpriteLine(x, xFrac, 0, gScreenHeight, visSprite, lightMulFrac);
                } else {
                    int32_t clip = top - visSprite.y1;      // Number of pixels to clip
                    int32_t run = bottom - top;             // Allowable run
                    
                    if (clip < 0) {     // Overrun?
                        run += clip;    // Remove from run
                        clip = 0;
                    }
                    
                    if (run > 0) {              // Still visible?
                        if (run > maxRun) {     // Too big?
                            run = maxRun;       // Force largest...
                        }
                        
                        oneSpriteLine(x, xFrac, top, bottom, visSprite, lightMulFrac);
                    }
                }
            }
        }
        xFrac += xStep;
    } while (++x <= x2);
}


//--------------------------------------------------------------------------------------------------
// Using a point in space, determine if it is BEHIND a wall.
// Use a cross product to determine facing.
//--------------------------------------------------------------------------------------------------
static bool segBehindPoint(const viswall_t& wall, const Fixed px, const Fixed py) noexcept {
    const seg_t& seg = *wall.SegPtr;    

    const Fixed sx1 = seg.v1.x;
    const Fixed sy1 = seg.v1.y;    
    const int32_t sdx = fixedToInt(seg.v2.x - sx1);
    const int32_t sdy = fixedToInt(seg.v2.y - sy1);
    const int32_t dx = fixedToInt(px - sx1);
    const int32_t dy = fixedToInt(py - sy1);
    
    const int32_t c1 = dx * sdy;
    const int32_t c2 = dy * sdx;
    return (c2 < c1);
}

//--------------------------------------------------------------------------------------------------
// See if a sprite needs clipping and if so, then draw it clipped
//--------------------------------------------------------------------------------------------------
void drawVisSprite(const vissprite_t& visSprite) noexcept {
    // Get the sprite's screen posts
    const int32_t x1 = std::max(visSprite.x1, 0);
    const int32_t x2 = std::min(visSprite.x2, (int32_t) gScreenWidth - 1);

    // Get the Z scale and assume not clipping initially
    const Fixed scaleFrac = visSprite.yscale;
    bool bIsClipped = false;

    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale is the clip seg!
    viswall_t* pDrawSeg = gpEndVisWall;

    while (pDrawSeg != gVisWalls) {
        --pDrawSeg;
        
        // Determine if the drawseg obscures the sprite
        if ((pDrawSeg->leftX > x2 || pDrawSeg->rightX < x1) ||
            (pDrawSeg->LargeScale <= scaleFrac) ||
            ((pDrawSeg->WallActions & (AC_TOPSIL|AC_BOTTOMSIL|AC_SOLIDSIL)) == 0)
        ) {
            continue;   // Doesn't cover sprite
        }

        if (pDrawSeg->SmallScale <= scaleFrac) {    // In range of the wall?
            if (segBehindPoint(*pDrawSeg, visSprite.thing->x, visSprite.thing->y)) {
                continue;   // Wall seg is behind sprite
            }
        }

        if (!bIsClipped) {  // Never initialized?
            bIsClipped = true;
            int32_t x = x1;
            uint32_t opening = gScreenHeight;
            do {
                gSprOpening[x] = opening;       // Init the clip table
            } while (++x <= x2);
        }

        const int32_t xl = (pDrawSeg->leftX < x1) ? x1 : pDrawSeg->leftX;       // Get the clip bounds
        const int32_t xr = (pDrawSeg->rightX > x2) ? x2 : pDrawSeg->rightX;

        // Clip this piece of the sprite
        const uint32_t silhouette = pDrawSeg->WallActions & (AC_TOPSIL|AC_BOTTOMSIL|AC_SOLIDSIL);
        int32_t x = xl;

        if (silhouette == AC_SOLIDSIL) {
            uint32_t opening = gScreenHeight << 8;
            do {
                gSprOpening[x] = opening;       // Clip these to blanks
            } while (++x <= xr);
            continue;
        }
        
        const uint8_t* const topsil = pDrawSeg->TopSil;
        const uint8_t* const bottomsil = pDrawSeg->BottomSil;

        if (silhouette == AC_BOTTOMSIL) {   // bottom sil only
            do {
                uint32_t opening = gSprOpening[x];
                if ((opening & 0xff) == gScreenHeight) {
                    gSprOpening[x] = (opening & 0xff00) + bottomsil[x];
                }
            } while (++x <= xr);
        } else if (silhouette == AC_TOPSIL) {   // top sil only
            do {
                uint32_t opening = gSprOpening[x];
                if ((opening & 0xff00) == 0) {
                    gSprOpening[x] = (topsil[x] << 8) + (opening & 0xff);
                }
            } while (++x <= xr);
        } else if (silhouette == (AC_TOPSIL|AC_BOTTOMSIL) ) {   // both
            do {
                int32_t top = gSprOpening[x];
                int32_t bottom = top & 0xff;
                top >>= 8;

                if (bottom == gScreenHeight) {
                    bottom = bottomsil[x];
                }

                if (!top) {
                    top = topsil[x];
                }
                gSprOpening[x] = (top << 8) + bottom;
            } while (++x <= xr);
        }
    }
    
    // Now that I have created the clip regions, let's see if I need to do this.
    // If no clipping is required then just draw normally.
    if (!bIsClipped) {
        drawSpriteNoClip(visSprite);
        return;
    }
    
    // Clip to screen coords
    const int32_t yt = std::max(visSprite.y1, 0);
    const int32_t yb = std::min(visSprite.y2, (int32_t) gScreenHeight);

    // Check the Y bounds to see if the clip rect even touches the sprite
    int32_t x = x1;

    do {
        int32_t top = gSprOpening[x];

        if (top == gScreenHeight)   // Clipped?
            continue;
        
        int32_t bottom = top & 0xff;
        top >>= 8;

        if (yt < top || yb >= bottom) {                 // Needs manual clipping!
            if (x != x1) {                              // Was any part visible?
                drawSpriteClip(x1, x2, visSprite);      // Draw it and exit
                return;
            }

            do {
                top = gSprOpening[x];
                if (top != (gScreenHeight << 8)) {
                    bottom = top & 0xff;
                    top >>= 8;
                    if (yt < bottom && yb >= top) {             // Is it even visible?
                        drawSpriteClip(x1, x2, visSprite);      // Draw it
                        return;
                    }
                }
            } while (++x <= x2);

            return;     // It's not visible at all!!
        }
    } while (++x <= x2);

    // It still didn't need clipping!
    // Draw the entire sprite without any clipping.
    drawSpriteNoClip(visSprite);    
}

//----------------------------------------------------------------------------------------------------------------------
// Sorts all sprites in the 3d view submitted to the renderer from back to front
//----------------------------------------------------------------------------------------------------------------------
static void sortAllMapObjectSprites() noexcept {
    std::sort(
        gVisSprites,
        gpEndVisSprite, 
        [](const vissprite_t& pSpr1, const vissprite_t& pSpr2) noexcept {
            return (pSpr1.yscale < pSpr2.yscale);
        }
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Draw all the sprites in the 3D view from back to front
//----------------------------------------------------------------------------------------------------------------------
void drawAllMapObjectSprites() noexcept {
    sortAllMapObjectSprites();

    const vissprite_t* pCurSprite = gVisSprites;
    const vissprite_t* const pEndSprite = gpEndVisSprite;

    while (pCurSprite < pEndSprite) {
        drawVisSprite(*pCurSprite);
        ++pCurSprite;
    }
}

END_NAMESPACE(Renderer)
