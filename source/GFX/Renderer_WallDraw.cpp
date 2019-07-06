#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Textures.h"
#include "Video.h"

//----------------------------------------------------------------------------------------------------------------------
// Code for drawing walls and skies in the game.
//
// Notes:
//  (1) Clip values are the solid pixel bounding the range
//  (2) floorclip starts out ScreenHeight
//  (3) ceilingclip starts out -1
//  (4) clipbounds[] = (ceilingclip +1 ) << 8 + floorclip
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Renderer)

static constexpr uint32_t OPENMARK = ((MAXSCREENHEIGHT - 1) << 8);

static uint32_t gClipBoundTop[MAXSCREENWIDTH];          // Bounds top y for vertical clipping
static uint32_t gClipBoundBottom[MAXSCREENWIDTH];       // Bounds bottom y for vertical clipping

struct drawtex_t {
    const std::byte*    data;               // Pointer to raw texture data
    uint32_t            width;              // Width of texture in pixels
    uint32_t            height;             // Height of texture in pixels
    int32_t             topheight;          // Top texture height in global pixels
    int32_t             bottomheight;       // Bottom texture height in global pixels
    uint32_t            texturemid;         // Anchor point for texture
};

static uint32_t gTexTextureColumn;      // Column offset into source image

//----------------------------------------------------------------------------------------------------------------------
// Draw a single column of a wall
//----------------------------------------------------------------------------------------------------------------------
static void drawWallColumn(
    const uint32_t viewX,
    const uint32_t viewY,
    const uint32_t Colnum,
    const uint32_t ColY,
    const uint32_t TexHeight,
    const std::byte* const Source,
    const uint32_t Run
) noexcept {
    // FIXME: CLEANUP AND OPTIMIZE!
    const uint32_t numPixels = (Run * gTexScale) >> SCALEBITS;
    const uint32_t numPixelsRounded = ((Run * gTexScale) & 0x1F0) != 0 ? numPixels + 1 : numPixels;    
    const Fixed lightMultiplier = getLightMultiplier(gTxTextureLight, MAX_WALL_LIGHT_VALUE);

    const uint16_t* const pPLUT = (const uint16_t*) Source;

    for (uint32_t pixNum = 0; pixNum < numPixelsRounded; ++pixNum) {
        const uint32_t dstY = viewY + pixNum;

        if (dstY >= 0 && dstY < gScreenHeight) {
            const uint32_t pixTexYOffsetFixed = (pixNum << (SCALEBITS + 1)) / gTexScale;
            const uint32_t pixTexYOffset = pixTexYOffsetFixed & 1 ? (pixTexYOffsetFixed / 2) + 1 : pixTexYOffsetFixed / 2;
            
            const uint32_t texYOffset = (ColY + pixTexYOffset) % (TexHeight);
            const uint32_t texOffset = Colnum + texYOffset;
            
            const uint8_t colorByte = (uint8_t) Source[32 + texOffset / 2];
            const uint8_t colorIdx = (texOffset & 1) != 0 ? (colorByte & 0x0F) : (colorByte & 0xF0) >> 4;
            
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
            const uint32_t screenX = viewX + gScreenXOffset;
            const uint32_t screenY = dstY + gScreenYOffset;

            Video::gFrameBuffer[screenY * Video::SCREEN_WIDTH + screenX] = finalColor;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Draws a single column of the sky
//----------------------------------------------------------------------------------------------------------------------
static void drawSkyColumn(const uint32_t viewX) noexcept {
    // Note: sky textures are 256 pixels wide so this wraps around
    const uint32_t colNum = (((gXToViewAngle[viewX] + gViewAngle) >> ANGLETOSKYSHIFT) & 0xFF);
    
    // Sky is always rendered at max light and 1.0 scale
    gTxTextureLight = MAX_WALL_LIGHT_VALUE << LIGHTSCALESHIFT;
    gTexScale = 1 << SCALEBITS;
    
    // Set source texture details
    // FIXME: don't keep doing this for each column
    const Texture* const pTexture = (const Texture*) getWallTexture(getCurrentSkyTexNum());
    const uint32_t texHeight = pTexture->height;

    drawWallColumn(
        viewX,
        0,
        colNum * texHeight,
        0,
        texHeight,
        pTexture->pData,
        texHeight
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Compute the screen location and part of the texture to use for the given draw texture and then draw a single wall
// column based on that information.
//----------------------------------------------------------------------------------------------------------------------
static void drawTexturedColumn(
    const drawtex_t& tex, 
    const uint32_t viewX
) noexcept {
    // Compute height of column from source image height and make sure not invalid
    const int32_t colHeightUnscaled = (tex.topheight - tex.bottomheight) >> HEIGHTBITS;     
    if (colHeightUnscaled <= 0) {
        return;
    }

    // View Y to draw at
    const uint32_t viewY = gCenterY - uint32_t((gTexScale * tex.topheight) >> (HEIGHTBITS + SCALEBITS));

    // Computing the column number in the texture
    uint32_t colnum = gTexTextureColumn;                                    // Get the starting column offset in the texture
    uint32_t frac = tex.texturemid - (tex.topheight << FIXEDTOHEIGHT);      // Get the anchor point
    frac >>= FRACBITS;

    while (frac & 0x8000) {
        --colnum;
        frac += tex.height;     // Make sure it's on the shape
    }

    frac &= 0x7f;                       // Zap unneeded bits
    colnum &= (tex.width - 1);          // Wrap around the texture
    colnum = (colnum * tex.height);     // Index to the shape

    // Draw the column
    drawWallColumn(
        viewX,
        viewY,
        colnum,
        frac,
        tex.height,
        tex.data,
        colHeightUnscaled
    );   
}

//----------------------------------------------------------------------------------------------------------------------
// Draw a single wall texture.
// Also save states for pending ceiling, floor and future clipping
//----------------------------------------------------------------------------------------------------------------------
static void drawSeg(const viswall_t& seg) noexcept {
    const uint32_t actionBits = seg.WallActions;
    
    if (actionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE)) {
        const uint32_t lightLevel = seg.seglightlevel;
        gLightMin = gLightMins[lightLevel];
        gLightMax = lightLevel;
        gLightSub = gLightSubs[lightLevel];
        gLightCoef = gLightCoefs[lightLevel];

        drawtex_t topTex;
        drawtex_t bottomTex;
        
        if (actionBits & AC_TOPTEXTURE) {   // Is there a top wall?
            const Texture* const pTex = seg.t_texture;

            topTex.topheight = seg.t_topheight;
            topTex.bottomheight = seg.t_bottomheight;
            topTex.texturemid = seg.t_texturemid;           
            topTex.width = pTex->width;
            topTex.height = pTex->height;
            topTex.data = pTex->pData;
        }
        
        if (actionBits & AC_BOTTOMTEXTURE) {  // Is there a bottom wall?
            const Texture* const pTex = seg.b_texture;

            bottomTex.topheight = seg.b_topheight;
            bottomTex.bottomheight = seg.b_bottomheight;
            bottomTex.texturemid = seg.b_texturemid;
            bottomTex.width = pTex->width;
            bottomTex.height = pTex->height;
            bottomTex.data = pTex->pData;
        }
        
        Fixed _scalefrac = seg.LeftScale;   // Init the scale fraction
        
        for (uint32_t x = seg.LeftX; x <= seg.RightX; ++x) {
            int32_t scale = _scalefrac >> FIXEDTOSCALE;     // Current scaling factor            
            if (scale >= 0x2000) {                          // Too large?
                scale = 0x1fff;                             // Fix the scale to maximum
            }
            
            // Calculate texture offset into shape
            gTexTextureColumn = (
                seg.offset - fixedMul(
                    gFineTangent[(seg.CenterAngle + gXToViewAngle[x]) >> ANGLETOFINESHIFT],
                    seg.distance
                )
            ) >> FRACBITS;
            
            gTexScale = scale;      // 0-0x1FFF
            
            {
                Fixed texturelight = ((scale * gLightCoef) >> 16) - gLightSub;                
                if (texturelight < gLightMin) {
                    texturelight = gLightMin;
                }                
                if (texturelight > gLightMax) {
                    texturelight = gLightMax;
                }                
                gTxTextureLight = texturelight;
            }
            
            // Daw the top and bottom textures (if present)
            if (actionBits & AC_TOPTEXTURE) {
                drawTexturedColumn(topTex, x);
            }
            if (actionBits & AC_BOTTOMTEXTURE) {
                drawTexturedColumn(bottomTex, x);
            }
            
            // Step to the next scale
            _scalefrac += seg.ScaleStep;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Given a span of pixels, see if it is already defined in a record somewhere.
// If it is, then merge it otherwise make a new plane definition.
//----------------------------------------------------------------------------------------------------------------------
static visplane_t* findPlane(
    visplane_t& plane,
    const Fixed height,
    const uint32_t picHandle,
    const int32_t start,
    const int32_t stop,
    const uint32_t light
) noexcept {
    visplane_t* pPlane = &plane + 1;    // Automatically skip to the next plane

    while (pPlane < gpEndVisPlane) {
        if ((height == pPlane->height) &&           // Same plane as before?
            (picHandle == pPlane->PicHandle) &&
            (light == pPlane->PlaneLight) &&
            (pPlane->open[start] == OPENMARK)       // Not defined yet?
        ) {
            if (start < pPlane->minx) {     // In range of the plane?
                pPlane->minx = start;       // Mark the new edge
            }
            if (stop > pPlane->maxx) {
                pPlane->maxx = stop;        // Mark the new edge
            }
            return pPlane;                  // Use the same one as before
        }

        ++pPlane;
    }
    
    // Make a new plane
    ASSERT_LOG(gpEndVisPlane - gVisPlanes < MAXVISPLANES, "No more visplanes!");
    pPlane = gpEndVisPlane;
    ++gpEndVisPlane;

    pPlane->height = height;            // Init all the vars in the visplane
    pPlane->PicHandle = picHandle;
    pPlane->minx = start;
    pPlane->maxx = stop;
    pPlane->PlaneLight = light;         // Set the light level

    // Quickly fill in the visplane table:
    // A brute force method to fill in the visplane record FAST!
    {
        uint32_t* pSet = pPlane->open;  

        for (uint32_t j = gScreenWidth / 8; j > 0; --j) {
            pSet[0] = OPENMARK;
            pSet[1] = OPENMARK;
            pSet[2] = OPENMARK;
            pSet[3] = OPENMARK;
            pSet[4] = OPENMARK;
            pSet[5] = OPENMARK;
            pSet[6] = OPENMARK;
            pSet[7] = OPENMARK;
            pSet += 8;
        }
    }

    return pPlane;
}

//----------------------------------------------------------------------------------------------------------------------
// Do a fake wall rendering so I can get all the visplane records.
// This is a fake-o routine so I can later draw the wall segments from back to front.
//----------------------------------------------------------------------------------------------------------------------
static void segLoop(const viswall_t& seg) noexcept {
    // Note: visplanes[0] is zero to force a FindPlane on the first pass    
    Fixed _scalefrac = seg.LeftScale;               // Init the scale fraction
    visplane_t* pFloorPlane = gVisPlanes;           // Reset the visplane pointers
    visplane_t* pCeilPlane = gVisPlanes;            // Reset the visplane pointers
    const uint32_t actionBits = seg.WallActions;

    for (uint32_t x = seg.LeftX; x <= seg.RightX; ++x) {
        int32_t scale = _scalefrac >> FIXEDTOSCALE;     // Current scaling factor
        if (scale >= 0x2000) {                          // Too large?
            scale = 0x1fff;                             // Fix the scale to maximum
        }

        const int32_t ceilingClipY = gClipBoundTop[x];      // Get the top y clip
        const int32_t floorClipY = gClipBoundBottom[x];     // Get the bottom y clip

        // Shall I add the floor?
        if (actionBits & AC_ADDFLOOR) {
            int32_t top = (int32_t) gCenterY - ((scale * seg.floorheight) >> (HEIGHTBITS + SCALEBITS));   // Y coord of top of floor
            if (top <= ceilingClipY) {
                top = ceilingClipY + 1;       // Clip the top of floor to the bottom of the visible area
            }
            int32_t bottom = floorClipY - 1;    // Draw to the bottom of the screen

            if (top <= bottom) {                            // Valid span?
                if (pFloorPlane->open[x] != OPENMARK) {     // Not already covered?
                    pFloorPlane = findPlane(
                        *pFloorPlane,
                        seg.floorheight,
                        seg.FloorPic,
                        x,
                        seg.RightX,
                        seg.seglightlevel
                    );
                }
                if (top) {
                    --top;
                }
                pFloorPlane->open[x] = (top << 8) + bottom;     // Set the new vertical span
            }
        }

        // Handle ceilings
        if (actionBits & AC_ADDCEILING) {
            int32_t top = ceilingClipY + 1;     // Start from the ceiling
            int32_t bottom = (int32_t) gCenterY - 1 - ((scale * seg.ceilingheight) >> (HEIGHTBITS + SCALEBITS));   // Bottom of the height
            
            if (bottom >= floorClipY) {     // Clip the bottom?
                bottom = floorClipY - 1;
            }

            if (top <= bottom) {                            // Valid span?
                if (pCeilPlane->open[x] != OPENMARK) {      // Already in use?
                    pCeilPlane = findPlane(
                        *pCeilPlane,
                        seg.ceilingheight,
                        seg.CeilingPic,
                        x,
                        seg.RightX,
                        seg.seglightlevel
                    );
                }
                if (top) {
                    --top;
                }
                pCeilPlane->open[x] = (top << 8) + bottom;      // Set the vertical span
            }
        }

        // Sprite clip sils
        if (actionBits & (AC_BOTTOMSIL|AC_NEWFLOOR)) {
            int32_t low = gCenterY - ((scale * seg.floornewheight) >> (HEIGHTBITS + SCALEBITS));
            if (low > floorClipY) {
                low = floorClipY;
            }
            if (low < 0) {
                low = 0;
            }
            if (actionBits & AC_BOTTOMSIL) {
                seg.BottomSil[x] = low;
            } 
            if (actionBits & AC_NEWFLOOR) {
                gClipBoundBottom[x] = low;
            }
        }

        if (actionBits & (AC_TOPSIL|AC_NEWCEILING)) {
            int32_t high = (gCenterY - 1) - ((scale * seg.ceilingnewheight) >> (HEIGHTBITS + SCALEBITS));
            if (high < ceilingClipY) {
                high = ceilingClipY;
            }
            if (high > (int32_t) gScreenHeight - 1) {
                high = gScreenHeight - 1;
            }
            if (actionBits & AC_TOPSIL) {
                seg.TopSil[x] = high + 1;
            }
            if (actionBits & AC_NEWCEILING) {
                gClipBoundTop[x] = high;
            }
        }

        // I can draw the sky right now!!
        if (actionBits & AC_ADDSKY) {
            int32_t bottom = gCenterY - ((scale * seg.ceilingheight) >> (HEIGHTBITS + SCALEBITS));
            if (bottom > floorClipY) {
                bottom = floorClipY;
            }

            if (ceilingClipY + 1 < bottom) {    // Valid?
                drawSkyColumn(x);               // Draw the sky
            }
        }

        // Step to the next scale
        _scalefrac += seg.ScaleStep;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Follow the list of walls and draw each and every wall fragment.
// Note : I draw the walls closest to farthest and I maintain a ZBuffer
//----------------------------------------------------------------------------------------------------------------------
void drawAllLineSegs() noexcept {
    // Init the vertical clipping records
    for (uint32_t i = 0; i < gScreenWidth; ++i) {
        gClipBoundTop[i] = -1;                  // Allow to the ceiling
        gClipBoundBottom[i] = gScreenHeight;    // Stop at the floor
    }

    // Process all the wall segments: create the visplanes and draw the sky only
    viswall_t* const pStartWall = gVisWalls;
    viswall_t* const pEndWall = gpEndVisWall;

    for (viswall_t* pWall = pStartWall; pWall < pEndWall; ++pWall) {
        segLoop(*pWall);
    }
    
    // Now I actually draw the walls back to front to allow for clipping because of slop.
    // Each wall is only drawn if needed...
    for (viswall_t* pWall = gpEndVisWall - 1; pWall >= pStartWall; --pWall) {
        drawSeg(*pWall);
    }
}

END_NAMESPACE(Renderer)
