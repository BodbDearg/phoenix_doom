#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Blit.h"
#include "Game/Data.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Sprites.h"
#include "Things/Info.h"
#include "Things/MapObj.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

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
    viewZOut = worldZ - gViewZ + SPRITE_EXTRA_Z_OFFSET;     // Node: have to apply a weird hack offset here (3DO Doom did this)

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

    uint32_t spriteResourceNum;
    uint32_t spriteFrameNum;
    state_t::decomposeSpriteFrameFieldComponents(
        pStatePtr->SpriteFrame,
        spriteResourceNum,
        spriteFrameNum,
        bIsSpriteFullBright
    );

    const uint8_t spriteAngle = getThingSpriteAngleForViewpoint(thing, viewXFrac, viewYFrac);

    // Load the current sprite for the thing and then the frame angle we want
    const Sprite* const pSprite = Sprites::load(spriteResourceNum);
    ASSERT(spriteFrameNum < pSprite->numFrames);
    ASSERT(spriteAngle < NUM_SPRITE_DIRECTIONS);

    const SpriteFrame* const pSpriteFrame = &pSprite->pFrames[spriteFrameNum];
    pSpriteFrameAngle = &pSpriteFrame->angles[spriteAngle];

    // Figure out other sprite flags
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
    const float screenW = (float) g3dViewWidth - 0.5f;
    const float screenH = (float) g3dViewHeight - 0.5f;

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
        sectorLightLevel = thing.subsector->sector->lightlevel + gExtraLight;
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
    const float worldX = fixed16ToFloat(thing.x);
    const float worldY = fixed16ToFloat(thing.y);
    const float worldZ = fixed16ToFloat(thing.z);

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
    drawSprite.pPixels = spriteFrameAngle->pTexture;
    drawSprite.worldX = worldX;
    drawSprite.worldY = worldY;
    drawSprite.screenLx = screenLx;
    drawSprite.screenRx = screenRx;
    drawSprite.screenTy = screenTy;
    drawSprite.screenBy = screenBy;
    drawSprite.depth = clipW;
    drawSprite.lightMul = lightMul;
    drawSprite.texW = spriteFrameAngle->width;
    drawSprite.texH = spriteFrameAngle->height;
    drawSprite.bFlip = spriteFrameAngle->flipped;
    drawSprite.bTransparent = bIsSpriteTransparent;
    
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
}

//----------------------------------------------------------------------------------------------------------------------
// Flip mode for a sprite
//----------------------------------------------------------------------------------------------------------------------
enum class SpriteFlipMode {
    NOT_FLIPPED,
    FLIPPED
};

//----------------------------------------------------------------------------------------------------------------------
// Clips the given sprite column against the given set of occluding columns.
// Modifies the given top and bottom y clip bounds.
//----------------------------------------------------------------------------------------------------------------------
static void clipSpriteFragmentAgainstOccludingCols(
    const SpriteFragment& frag,
    const OccludingColumns& cols,
    int16_t& yClipT,
    int16_t& yClipB
) noexcept {
    const uint32_t validCount = gValidCount;
    const uint32_t numCols = cols.count;
    BLIT_ASSERT(numCols <= OccludingColumns::MAX_ENTRIES);
    
    for (uint32_t i = 0; i < numCols; ++i) {
        // Grab the line associated with this column and see if we did an 'in front' test against this column
        line_t& line = *cols.pLines[i];

        if (line.validCount != validCount) {
            // Get the min and max depths of the line. These are used for tests that take precedence over
            // the cross-product test determining whether the sprite is in front of the line:
            //
            //  (1) If the sprite is deeper than the max line depth then it must be clipped by the line.
            //      This handles cases where the sprite is past a corner but technically in 'front' of the line.
            //  (2) If the sprite is closer than the min line depth then it must be in front of the line.
            //      This handles cases where the sprite is in front of the line but technically 'behind' it.
            //
            // These extra checks help produce clipping that works in a similar way to software rendered Doom,
            // with all of the same artifacts and corner cases too... Generally it works better than just a
            // standard Z test since it avoids lots of problems with sprites poking into walls. It still has
            // issues in some places however with parallel lines that are subdivided, often a sprite will be
            // seen to be clipped at the subdivisions...
            //
            const float lineMinDepth = std::min(line.v1DrawDepth, line.v2DrawDepth);
            const float lineMaxDepth = std::max(line.v1DrawDepth, line.v2DrawDepth);

            if (frag.depth > lineMaxDepth) {
                line.bIsInFrontOfSprite = true;
            } 
            else if (frag.depth < lineMinDepth) {
                line.bIsInFrontOfSprite = false;
            }
            else {
                // Okay, this is where we do the magic cross product check to see if the sprite is in 'front' of the line.
                // This is the same method as the 'SegBehindPoint' function in the original 3DO Doom code:
                float spriteRx, spriteRy, lineDx, lineDy;

                if (line.drawnSideIndex == 0) {
                    spriteRx = frag.spriteWorldX - line.v1f.x;
                    spriteRy = frag.spriteWorldY - line.v1f.y;
                    lineDx = line.v2f.x - line.v1f.x;
                    lineDy = line.v2f.y - line.v1f.y;
                } else {
                    spriteRx = frag.spriteWorldX - line.v2f.x;
                    spriteRy = frag.spriteWorldY - line.v2f.y;
                    lineDx = line.v1f.x - line.v2f.x;
                    lineDy = line.v1f.y - line.v2f.y;
                }

                const float a = spriteRx * lineDy;
                const float b = spriteRy * lineDx;
                line.bIsInFrontOfSprite = (a < b);
            }

            // Don't run this calculation again for this sprite
            line.validCount = validCount;
        }

        // Ignore this line if it is not in front of the sprite
        if (!line.bIsInFrontOfSprite)
            continue;
        
        // This line occludes the sprite: update the clip bounds
        const OccludingColumns::Bounds bounds = cols.bounds[i];
        yClipT = std::max(yClipT, bounds.top);
        yClipB = std::min(yClipB, bounds.bottom);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Clips and draws a single sprite fragment
//----------------------------------------------------------------------------------------------------------------------
static void clipAndDrawSpriteFragment(const SpriteFragment& frag) noexcept {
    BLIT_ASSERT(frag.x < g3dViewWidth);

    // Firstly figure out the top and bottom clip bounds for the sprite fragment
    int16_t yClipT = -1;
    int16_t yClipB = (int16_t) g3dViewHeight;

    {
        const OccludingColumns& occludingCols = gOccludingCols[frag.x];
        clipSpriteFragmentAgainstOccludingCols(frag, occludingCols, yClipT, yClipB);
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
    if (!frag.isTransparent) {
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
            Video::gpFrameBuffer + (uintptr_t) g3dViewYOffset * Video::gScreenWidth + g3dViewXOffset,
            g3dViewWidth,
            g3dViewHeight,
            Video::gScreenWidth,
            frag.x,
            dstY,
            dstCount,
            0.0f,
            frag.texYStep,
            frag.lightMul,
            frag.lightMul,
            frag.lightMul
        );
    } else {
        Blit::blitColumn<
            Blit::BCF_STEP_Y |
            Blit::BCF_ALPHA_TEST |
            Blit::BCF_ALPHA_BLEND |
            Blit::BCF_COLOR_MULT_RGB |
            Blit::BCF_COLOR_MULT_A |
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
            Video::gpFrameBuffer + (uintptr_t) g3dViewYOffset * Video::gScreenWidth + g3dViewXOffset,
            g3dViewWidth,
            g3dViewHeight,
            Video::gScreenWidth,
            frag.x,
            dstY,
            dstCount,
            0.0f,
            frag.texYStep,
            frag.lightMul * MF_SHADOW_COLOR_MULT,
            frag.lightMul * MF_SHADOW_COLOR_MULT,
            frag.lightMul * MF_SHADOW_COLOR_MULT,
            MF_SHADOW_ALPHA
        );
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Emit the sprite fragments for one draw sprite
//----------------------------------------------------------------------------------------------------------------------
template <SpriteFlipMode FLIP_MODE>
static void drawSprite(const DrawSprite& sprite) noexcept {
    BLIT_ASSERT(sprite.screenRx >= sprite.screenLx);
    BLIT_ASSERT(sprite.screenBy >= sprite.screenTy);

    // Figure out the size of the sprite on the screen
    const float spriteW = sprite.screenRx - sprite.screenLx;
    const float spriteH = sprite.screenBy - sprite.screenTy;

    int32_t spriteLxInt = (int32_t) sprite.screenLx;
    int32_t spriteRxInt = (int32_t) sprite.screenRx;
    int32_t spriteTyInt = (int32_t) sprite.screenTy;
    int32_t spriteByInt = (int32_t) sprite.screenBy + 2;    // Do 2 extra rows to ensure we capture sprite borders!

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
    const float texSubPixelXAdjust = -(sprite.screenLx - std::trunc(sprite.screenLx)) * texXStep;
    const float texSubPixelYAdjust = -(sprite.screenTy - std::trunc(sprite.screenTy)) * texYStep;

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

    if (spriteRxInt >= (int32_t) g3dViewWidth) {
        endScreenX = (int32_t) g3dViewWidth;
        bDoExtraCol = false;
    } else {
        endScreenX = spriteRxInt + 1;
    }

    // Increment this marker for clipping checks
    ++gValidCount;

    // Emit the columns
    {
        float texXf;

        if constexpr (FLIP_MODE == SpriteFlipMode::FLIPPED) {
            texXf = std::nextafterf(texW, 0.0f);
        } else {
            texXf = 0.0f;
        }

        while (curScreenX < endScreenX) {
            BLIT_ASSERT(curScreenX >= 0 && curScreenX < (int32_t) g3dViewWidth);
            const uint16_t texX = (uint16_t) texXf;

            if (texX >= texW)
                break;

            SpriteFragment frag;
            frag.x = (uint16_t) curScreenX;
            frag.y = (uint16_t) spriteTyInt;
            frag.height = (uint16_t) spriteHInt;
            frag.texH = (uint16_t) texHInt;
            frag.isTransparent = (sprite.bTransparent) ? 1 : 0;
            frag.depth = sprite.depth;
            frag.lightMul = sprite.lightMul;
            frag.texYStep = texYStep;
            frag.texYSubPixelAdjust = texSubPixelYAdjust;
            frag.pSpriteColPixels = sprite.pPixels + (uintptr_t) texX * texHInt;
            frag.spriteWorldX = sprite.worldX;
            frag.spriteWorldY = sprite.worldY;

            clipAndDrawSpriteFragment(frag);

            ++curScreenX;
            ++curColNum;

            if constexpr (FLIP_MODE == SpriteFlipMode::FLIPPED) {
                texXf = texW - std::max(texXStep * (float) curColNum + texSubPixelXAdjust, 0.5f);
            } else {
                texXf = std::max(texXStep * (float) curColNum + texSubPixelXAdjust, 0.0f);
            }
        }
    }

    // Do an extra column at the end if required
    if (bDoExtraCol) {
        curScreenX = spriteRxInt + 1;

        if (curScreenX < (int32_t) g3dViewWidth) {
            const uint16_t texX = (FLIP_MODE == SpriteFlipMode::FLIPPED) ? 0 : texWInt - 1;

            SpriteFragment frag;
            frag.x = (uint16_t) curScreenX;
            frag.y = (uint16_t) spriteTyInt;
            frag.height = (uint16_t) spriteHInt;
            frag.texH = (uint16_t) texHInt;
            frag.isTransparent = (sprite.bTransparent) ? 1 : 0;
            frag.depth = sprite.depth;
            frag.lightMul = sprite.lightMul;
            frag.texYStep = texYStep;
            frag.texYSubPixelAdjust = texSubPixelYAdjust;
            frag.pSpriteColPixels = sprite.pPixels + (uintptr_t) texX * texHInt;
            frag.spriteWorldX = sprite.worldX;
            frag.spriteWorldY = sprite.worldY;

            clipAndDrawSpriteFragment(frag);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Draw all the sprites in the 3D view from back to front
//----------------------------------------------------------------------------------------------------------------------
void drawAllSprites() noexcept {
    sortAllSprites();

    for (const DrawSprite& sprite : gDrawSprites) {
        if (sprite.bFlip) {
            drawSprite<SpriteFlipMode::FLIPPED>(sprite);
        } else {
            drawSprite<SpriteFlipMode::NOT_FLIPPED>(sprite);
        }
    }
}

END_NAMESPACE(Renderer)
