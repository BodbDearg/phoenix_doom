#include "Renderer_Internal.h"

#include "Base/FMath.h"
#include "Base/Tables.h"
#include "Blit.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Sprites.h"
#include "Things/Info.h"
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
// Transforms a sprite world position into viewspace and tells if it should be culled due to being behind the camera
//----------------------------------------------------------------------------------------------------------------------
static inline void transformWorldCoordsToViewSpace(
    const float worldX,
    const float worldY,
    const float worldZ,
    float& viewXOut,
    float& viewYOut,
    float& viewZOut,
    bool& shouldCullSprite
) noexcept {
    // Transform by view position first
    const float translatedX = worldX - gViewX;
    const float translatedY = worldY - gViewY;
    viewZOut = worldZ - gViewZ;

    // Do 2D rotation by view angle.
    // Rotation matrix formula from: https://en.wikipedia.org/wiki/Rotation_matrix
    const float viewCos = gViewCos;
    const float viewSin = gViewSin;
    viewXOut = viewCos * translatedX - viewSin * translatedY;
    viewYOut = viewSin * translatedX + viewCos * translatedY;

    // Cull if behind the near plane!
    shouldCullSprite = (viewYOut <= Z_NEAR);
}

//----------------------------------------------------------------------------------------------------------------------
// Convert the left and right coordinates of the sprite to clip space and return the 'w' value (depth).
// Also tells if we should cull the sprite due to it being off screen.
//----------------------------------------------------------------------------------------------------------------------
static void transformSpriteXBoundsAndWToClipSpace(
    const float viewLx,
    const float viewRx,
    const float viewY,
    float& clipLxOut,
    float& clipRxOut,
    float& clipWOut,
    bool& shouldCullSprite
) noexcept {
    // Notes:
    //  (1) We treat 'y' as if it were 'z' for the purposes of these calculations, since the
    //      projection matrix has 'z' as the depth value and not y (Doom coord sys). 
    //  (2) We assume that the sprite always starts off with an implicit 'w' value of '1'.
    clipLxOut = viewLx * gProjMatrix.r0c0;
    clipRxOut = viewRx * gProjMatrix.r0c0;
    clipWOut = viewY;   // Note: r3c2 is an implicit 1.0 - hence we just do this!

    // A screen coordinate will be onscreen if the coord is >= -w and <= w
    shouldCullSprite = (
        (clipLxOut > clipWOut) ||
        (clipRxOut < -clipWOut)
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Get the sprite angle (0-7) to render a thing with for the given viewpoint
//----------------------------------------------------------------------------------------------------------------------
static uint8_t getThingSpriteAngleForViewpoint(const mobj_t& thing, const Fixed viewX, const Fixed viewY) noexcept {
    angle_t ang = PointToAngle(viewX, viewY, thing.x, thing.y);     // Get angle to thing
    ang -= thing.angle;                                             // Adjust for which way the thing is facing
    const uint8_t angleIdx = (ang + (ANG45 / 2) * 9U) >> 29;        // Compute and return angle index (0-7)
    return angleIdx;
}

//----------------------------------------------------------------------------------------------------------------------
// Gets the sprite frame for the given map thing and figures out if we need to draw full bright or transparent
//----------------------------------------------------------------------------------------------------------------------
void getSpriteDetailsForMapObj(
    const mobj_t& thing,
    const Fixed viewXFrac,
    const Fixed viewYFrac,
    const SpriteFrameAngle*& pSpriteFrameAngle,
    bool& bIsSpriteFullBright,
    bool& bIsSpriteTransparent
) noexcept {
    // Figure out the sprite that we want
    const state_t* const pStatePtr = thing.state;
    const uint32_t spriteResourceNum = pStatePtr->SpriteFrame >> FF_SPRITESHIFT;
    const uint32_t spriteFrameNum = pStatePtr->SpriteFrame & FF_FRAMEMASK;
    const uint8_t spriteAngle = getThingSpriteAngleForViewpoint(thing, viewXFrac, viewYFrac);
    
    // Load the current sprite for the thing and then the frame angle we want
    const Sprite* const pSprite = loadSprite(spriteResourceNum);
    ASSERT(spriteFrameNum < pSprite->numFrames);
    ASSERT(spriteAngle < NUM_SPRITE_DIRECTIONS);

    const SpriteFrame* const pSpriteFrame = &pSprite->pFrames[spriteFrameNum];
    pSpriteFrameAngle = &pSpriteFrame->angles[spriteAngle];

    // Figure out other sprite flags
    bIsSpriteFullBright = ((pStatePtr->SpriteFrame & FF_FULLBRIGHT) != 0);
    bIsSpriteTransparent = ((thing.flags & MF_SHADOW) != 0);
}

//----------------------------------------------------------------------------------------------------------------------
// Convert the top and bottom coordinates of the sprite to clip space.
// Also tells if we should cull the sprite due to it being off screen.
//----------------------------------------------------------------------------------------------------------------------
static void transformSpriteZValuesToClipSpace(
    const float viewTz,
    const float viewBz,
    const float clipW,
    float& clipTz,
    float& clipBz,
    bool& shouldCullSprite
) noexcept {
    clipTz = viewTz * gProjMatrix.r1c1;
    clipBz = viewBz * gProjMatrix.r1c1;

    // A screen coordinate will be onscreen if the coord is >= -w and <= w
    shouldCullSprite = (
        (clipTz > clipW) ||
        (clipBz < -clipW)
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Transforms the sprite coordinates to screen space (pixel values)
//----------------------------------------------------------------------------------------------------------------------
static void transformSpriteCoordsToScreenSpace(
    const float clipLx,
    const float clipRx,
    const float clipTz,
    const float clipBz,
    const float clipW,
    float& screenLx,
    float& screenRx,
    float& screenTy,
    float& screenBy
) noexcept {
    // Note: have to subtract a bit here because at 100% of the range we don't want to be >= screen width or height!
    const float screenW = (float) gScreenWidth - 0.5f;
    const float screenH = (float) gScreenHeight - 0.5f;

    // Do perspective division for all the coordinates to transform into normalized device coords
    const float clipInvW = 1.0f / clipW;
    const float ndcLx = clipLx * clipInvW;
    const float ndcRx = clipRx * clipInvW;
    const float ndcTz = clipTz * clipInvW;
    const float ndcBz = clipBz * clipInvW;

    // All coords are in the range -1 to +1 now.
    // Bring in the range 0-1 and then expand to screen width and height:
    screenLx = (ndcLx * 0.5f + 0.5f) * screenW;
    screenRx = (ndcRx * 0.5f + 0.5f) * screenW;
    screenTy = (ndcTz * 0.5f + 0.5f) * screenH;
    screenBy = (ndcBz * 0.5f + 0.5f) * screenH;
}

//----------------------------------------------------------------------------------------------------------------------
// Determine the light multiplier for the given thing
//----------------------------------------------------------------------------------------------------------------------
float determineLightMultiplierForThing(const mobj_t& thing, const bool bIsFullBright, const float depth) noexcept {    
    uint32_t sectorLightLevel;

    if (bIsFullBright) {
        sectorLightLevel = 255;
    } else {
        sectorLightLevel = thing.subsector->sector->lightlevel;
    }

    const LightParams lightParams = getLightParams(sectorLightLevel);
    return lightParams.getLightMulForDist(depth);
}

//----------------------------------------------------------------------------------------------------------------------
// Generate a 'DrawSprite' for the given thing and add to the list for this frame.
// Does basic culling as well also.
//----------------------------------------------------------------------------------------------------------------------
void addSpriteToFrame(const mobj_t& thing) noexcept {
    // The player never gets added for obvious reasons
    if (thing.player)
        return;
    
    // Get the world position of the thing firstly
    const float worldX = FMath::doomFixed16ToFloat<float>(thing.x);
    const float worldY = FMath::doomFixed16ToFloat<float>(thing.y);
    const float worldZ = FMath::doomFixed16ToFloat<float>(thing.z);
    
    // Convert to viewspace and cull if that transform determined the sprite is behind the camera
    float viewX;
    float viewY;
    float viewZ;
    bool bCullSprite;
    transformWorldCoordsToViewSpace(worldX, worldY, worldZ, viewX, viewY, viewZ, bCullSprite);

    if (bCullSprite)
        return;
    
    // Figure out what sprite, frame and frame angle we want
    bool bIsSpriteFullBright;
    bool bIsSpriteTransparent;
    const SpriteFrameAngle* spriteFrameAngle;
    getSpriteDetailsForMapObj(thing, gViewXFrac, gViewYFrac, spriteFrameAngle, bIsSpriteFullBright, bIsSpriteTransparent);

    ASSERT(spriteFrameAngle->width > 0);
    ASSERT(spriteFrameAngle->height > 0);

    // Figure out the clip space left and right coords for the sprite.
    // Again, cull if it is entirely offscreen!
    const float texW = spriteFrameAngle->width;
    const float texH = spriteFrameAngle->height;
    const float viewLx = viewX - (float) spriteFrameAngle->leftOffset;
    const float viewRx = viewLx + texW;

    float clipLx;
    float clipRx;
    float clipW;
    transformSpriteXBoundsAndWToClipSpace(viewLx, viewRx, viewY, clipLx, clipRx, clipW, bCullSprite);

    if (bCullSprite)
        return;

    // Get the view space z coordinates for the sprite, transform to clip space and potentially cull
    const float viewTz = viewZ + (float) spriteFrameAngle->topOffset;
    const float viewBz = viewTz - texH;

    float clipTz;
    float clipBz;
    transformSpriteZValuesToClipSpace(viewTz, viewBz, clipW, clipTz, clipBz, bCullSprite);

    if (bCullSprite)
        return;

    // Transform the sprite coordinates into screen space
    float screenLx;
    float screenRx;
    float screenTy;
    float screenBy;
    transformSpriteCoordsToScreenSpace(clipLx, clipRx, clipTz, clipBz, clipW, screenLx, screenRx, screenTy, screenBy);

    // Determine the light multiplier for the sprite
    const float lightMul = determineLightMultiplierForThing(thing, bIsSpriteFullBright, clipW);

    // Makeup the draw sprite and add to the list
    DrawSprite drawSprite;
    drawSprite.depth = clipW;
    drawSprite.lx = screenLx;
    drawSprite.rx = screenRx;
    drawSprite.ty = screenTy;
    drawSprite.by = screenBy;
    drawSprite.lightMul = lightMul;
    drawSprite.bFlip = spriteFrameAngle->flipped;
    drawSprite.bTransparent = bIsSpriteTransparent;
    drawSprite.texW = spriteFrameAngle->width;
    drawSprite.texH = spriteFrameAngle->height;
    drawSprite.pPixels = spriteFrameAngle->pTexture;

    gDrawSprites.push_back(drawSprite);
}

//----------------------------------------------------------------------------------------------------------------------
// Sorts all sprites in the 3d view submitted to the renderer from back to front
//----------------------------------------------------------------------------------------------------------------------
static void sortAllSprites() noexcept {
    std::sort(
        gDrawSprites.begin(),
        gDrawSprites.end(),
        [](const DrawSprite& s1, const DrawSprite& s2) noexcept {
            return (s1.depth > s2.depth);
        }
    );

    /* TODO: REMOVE
    std::sort(
        gVisSprites,
        gpEndVisSprite, 
        [](const vissprite_t& pSpr1, const vissprite_t& pSpr2) noexcept {
            return (pSpr1.yscale < pSpr2.yscale);
        }
    );
    */
}

//----------------------------------------------------------------------------------------------------------------------
// Flip mode for a sprite
//----------------------------------------------------------------------------------------------------------------------
enum class SpriteFlipMode {
    NOT_FLIPPED,
    FLIPPED
};

//----------------------------------------------------------------------------------------------------------------------
// Emit the sprite fragments for one draw sprite
//----------------------------------------------------------------------------------------------------------------------
template <SpriteFlipMode FLIP_MODE>
static void emitFragmentsForSprite(const DrawSprite& sprite) noexcept {
    BLIT_ASSERT(sprite.rx >= sprite.lx);
    BLIT_ASSERT(sprite.by >= sprite.ty);

    // Figure out the size of the sprite on the screen
    const float spriteW = sprite.rx - sprite.lx + 1.0f;
    const float spriteH = sprite.by - sprite.ty + 1.0f;

    int32_t spriteLxInt = (int32_t) sprite.lx;
    int32_t spriteRxInt = (int32_t) sprite.rx;
    int32_t spriteTyInt = (int32_t) sprite.ty;
    int32_t spriteByInt = (int32_t) sprite.by + 2;      // Do 2 extra rows to ensure we capture sprite borders!

    int32_t spriteWInt = spriteRxInt - spriteLxInt + 1;
    int32_t spriteHInt = spriteByInt - spriteTyInt + 1;

    // Figure out x and y texcoord stepping
    const uint16_t texWInt = sprite.texW;
    const uint16_t texHInt = sprite.texH;
    const float texW = texWInt;
    const float texH = texHInt;
    const float texXStep = (spriteW > 1) ? texW / spriteW : 0.0f;
    const float texYStep = (spriteH > 1) ? texH / spriteH : 0.0f;
    
    // Computing a sub pixel x and y adjustment for stability. This is similar to the adjustment we do for walls.
    // We take into account the fractional part of the first pixel when computing the distance to travel to the 2nd pixel.
    const float texSubPixelXAdjust = -(sprite.lx - std::trunc(sprite.lx)) * texXStep;
    const float texSubPixelYAdjust = -(sprite.ty - std::trunc(sprite.ty)) * texYStep;

    // If we fall short of displaying the last column in a sprite then extend it by 1 column.
    // This helps ensure that sprites don't get a column of pixels cut off at the end and miss important borders/edges.
    bool bDoExtraCol;

    {
        const float origEndTexX = (float)(spriteWInt - 1) * texXStep + texSubPixelXAdjust;
        const float texXShortfall = texW - 1.0f - origEndTexX;
        bDoExtraCol = (texXShortfall > 0.0f);
    }

    // Skip past columns that are on the left side of the screen
    int32_t curScreenX = spriteLxInt;
    uint32_t curColNum = 0;

    if (curScreenX < 0) {
        curColNum = -curScreenX;
        curScreenX = 0;
    }

    // Safety, this shouldn't be required but just in case if we are TOO MUCH off screen
    if ((int32_t) curColNum >= spriteWInt)
        return;
    
    // Ensure we don't go past the right side of the screen.
    // If we do also cancel any extra columns we might have ordered:
    int32_t endScreenX;

    if (spriteRxInt >= (int32_t) gScreenWidth) {
        endScreenX = (int32_t) gScreenWidth;
        bDoExtraCol = false;
    } else {
        endScreenX = spriteRxInt + 1;
    }

    // Emit the columns
    {
        float texXf;

        if constexpr (FLIP_MODE == SpriteFlipMode::FLIPPED) {
            texXf = std::nextafterf(texW, 0.0f);
        } else {
            texXf = 0.0f;
        }

        while (curScreenX < endScreenX) {
            BLIT_ASSERT(curScreenX >= 0 && curScreenX < (int32_t) gScreenWidth);
            const uint16_t texX = (uint16_t) texXf;

            if (texX >= texW)
                break;

            SpriteFragment frag;
            frag.x = curScreenX;
            frag.y = spriteTyInt;
            frag.height = spriteHInt;
            frag.texH = texHInt;
            frag.isTransparent = (sprite.bTransparent) ? 1 : 0;
            frag.depth = sprite.depth;
            frag.lightMul = sprite.lightMul;
            frag.texYStep = texYStep;
            frag.texYSubPixelAdjust = texSubPixelYAdjust;
            frag.pSpriteColPixels = sprite.pPixels + texX * texHInt;

            gSpriteFragments.push_back(frag);

            ++curScreenX;
            ++curColNum;

            if constexpr (FLIP_MODE == SpriteFlipMode::FLIPPED) {
                texXf = texW - std::max(texXStep * (float) curColNum + texSubPixelXAdjust, 0.5f);
            } else {
                texXf = std::fmax(texXStep * (float) curColNum + texSubPixelXAdjust, 0.0f);
            }
        }
    }

    // Do an extra column at the end if required
    if (bDoExtraCol) {
        curScreenX = spriteRxInt + 1;

        if (curScreenX < (int32_t) gScreenWidth) {
            const uint16_t texX = (FLIP_MODE == SpriteFlipMode::FLIPPED) ? 0 : texWInt - 1;

            SpriteFragment frag;
            frag.x = curScreenX;
            frag.y = spriteTyInt;
            frag.height = spriteHInt;
            frag.texH = texHInt;
            frag.isTransparent = (sprite.bTransparent) ? 1 : 0;
            frag.depth = sprite.depth;
            frag.lightMul = sprite.lightMul;
            frag.texYStep = texYStep;
            frag.texYSubPixelAdjust = texSubPixelYAdjust;
            frag.pSpriteColPixels = sprite.pPixels + texX * texHInt;

            gSpriteFragments.push_back(frag);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Draws a single sprite fragment
//----------------------------------------------------------------------------------------------------------------------
void drawSpriteFragment(const SpriteFragment frag) noexcept {
    BLIT_ASSERT(frag.x < gScreenWidth);

    // Firstly figure out the top and bottom clip bounds for the sprite fragment    
    int16_t yClipT = -1;
    int16_t yClipB = gScreenHeight;

    {
        const OccludingColumns& occludingCols = gOccludingCols[frag.x];
        const uint32_t numOccludingCols = occludingCols.count;
        BLIT_ASSERT(numOccludingCols <= OccludingColumns::MAX_ENTRIES);

        for (uint32_t i = 0; i < numOccludingCols; ++i) {
            // Ignore if the sprite is in front!
            if (frag.depth <= occludingCols.depths[i])
                continue;
            
            // Update the clip bounds
            const OccludingColumns::Bounds bounds = occludingCols.bounds[i];
            yClipT = std::max(yClipT, bounds.top);
            yClipB = std::min(yClipB, bounds.bottom);
        }
    }

    // If we are drawing nothing then bail
    if (yClipT >= yClipB)
        return;

    // Do clipping against the top of the bounds
    float srcTexY = 0.0f;
    float srcTexYSubPixelAdjust = frag.texYSubPixelAdjust;
    uint32_t dstY = frag.y;
    uint32_t dstCount = frag.height;

    if ((int32_t) dstY <= yClipT) {
        const uint32_t numPixelsOffscreen = (uint32_t)(yClipT - (int32_t) dstY + 1);

        if (numPixelsOffscreen >= dstCount)
            return;
        
        srcTexY = frag.texYStep * (float) numPixelsOffscreen + srcTexYSubPixelAdjust;
        srcTexYSubPixelAdjust = 0.0f;
        dstY += numPixelsOffscreen;
        dstCount -= numPixelsOffscreen;
    }

    // Do clipping against the bottom of the bounds
    {
        const uint32_t endY = dstY + dstCount;

        if ((int32_t) endY > yClipB) {
            const uint32_t numPixelsOffscreen = (uint32_t)((int32_t) endY - yClipB);

            if (numPixelsOffscreen >= dstCount)
                return;

            dstCount -= numPixelsOffscreen;
        }
    }

    // Draw the actual sprite column
    Blit::blitColumn<
        Blit::BCF_STEP_Y |
        Blit::BCF_ALPHA_TEST |
        Blit::BCF_COLOR_MULT_RGB |
        Blit::BCF_V_WRAP_DISCARD |
        Blit::BCF_V_CLIP
    >(
        frag.pSpriteColPixels,
        1,
        frag.texH,
        0.0f,
        srcTexY,
        0.0f,
        srcTexYSubPixelAdjust,
        Video::gFrameBuffer + gScreenYOffset * Video::SCREEN_WIDTH + gScreenXOffset,
        gScreenWidth,
        gScreenHeight,
        Video::SCREEN_WIDTH,
        frag.x,
        dstY,
        dstCount,
        0.0f,
        frag.texYStep,
        frag.lightMul,
        frag.lightMul,
        frag.lightMul
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Draw all the sprites in the 3D view from back to front
//----------------------------------------------------------------------------------------------------------------------
void drawAllSprites() noexcept {    
    sortAllSprites();

    for (const DrawSprite& sprite : gDrawSprites) {
        if (sprite.bFlip) {
            emitFragmentsForSprite<SpriteFlipMode::FLIPPED>(sprite);
        } else {
            emitFragmentsForSprite<SpriteFlipMode::NOT_FLIPPED>(sprite);
        }
    }

    for (const SpriteFragment& spriteFrag : gSpriteFragments) {
        drawSpriteFragment(spriteFrag);
    }

    /* TODO: REMOVE
    const vissprite_t* pCurSprite = gVisSprites;
    const vissprite_t* const pEndSprite = gpEndVisSprite;

    while (pCurSprite < pEndSprite) {
        drawVisSprite(*pCurSprite);
        ++pCurSprite;
    }
    */
}

END_NAMESPACE(Renderer)
