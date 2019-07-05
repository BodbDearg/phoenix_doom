#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Textures.h"
#include "ThreeDO.h"
#include "Video.h"
#include <cstddef>

#define OPENMARK ((MAXSCREENHEIGHT-1)<<8)

BEGIN_NAMESPACE(Renderer)

/**********************************

    This code will draw all the VERTICAL walls for
    a screen.

    Clip values are the solid pixel bounding the range
    floorclip starts out ScreenHeight
    ceilingclip starts out -1
    clipbounds[] = (ceilingclip+1)<<8 + floorclip

**********************************/

static uint32_t gClipBoundTop[MAXSCREENWIDTH];          // Bounds top y for vertical clipping
static uint32_t gClipBoundBottom[MAXSCREENWIDTH];       // Bounds bottom y for vertical clipping

struct drawtex_t{
    const std::byte*    data;               // Pointer to raw texture data
    uint32_t            width;              // Width of texture in pixels
    uint32_t            height;             // Height of texture in pixels
    int32_t             topheight;          // Top texture height in global pixels
    int32_t             bottomheight;       // Bottom texture height in global pixels
    uint32_t            texturemid;         // Anchor point for texture
};

static drawtex_t gTopTex;               // Describe the upper texture
static drawtex_t gBottomTex;            // Describe the lower texture
static uint32_t gTexTextureColumn;      // Column offset into source image

/**********************************

    Drawing the wall columns are a little trickier, so I'll do this next.
    The parms are, 
    gTexX = screen x coord for the virtual screen.
    y = screen y coord for the virtual screen.
    bottom = screen y coord for the BOTTOM of the pixel run. Subtract from top
        to get the exact destination pixel run count.
    colnum = index for which scan line to draw from the source image. Note that
        this number has all bits set so I must mask off the unneeded bits for
        texture wraparound.
        
    No light shading is used. The scale factor is a constant.
    
**********************************/
static void DrawWallColumn(
    const uint32_t y,
    const uint32_t Colnum,
    const uint32_t ColY,
    const uint32_t TexHeight,
    const std::byte* const Source,
    const uint32_t Run
)
{
    // TODO: TEMP - CLEANUP
    const uint32_t numPixels = (Run * gTexScale) >> SCALEBITS;
    const uint32_t numPixelsRounded = ((Run * gTexScale) & 0x1F0) != 0 ? numPixels + 1 : numPixels;    
    const Fixed lightMultiplier = getLightMultiplier(gTxTextureLight, MAX_WALL_LIGHT_VALUE);

    const uint16_t* const pPLUT = (const uint16_t*) Source;

    for (uint32_t pixNum = 0; pixNum < numPixelsRounded; ++pixNum) {
        const uint32_t dstY = y + pixNum;
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
            const uint32_t screenX = gTexX + gScreenXOffset;
            const uint32_t screenY = dstY + gScreenYOffset;

            Video::gFrameBuffer[screenY * Video::SCREEN_WIDTH + screenX] = finalColor;
        }
    }
    
    // DC: FIXME: implement/replace
    #if 0
        MyCCB* DestCCB;         // Pointer to new CCB entry 
        Word Colnum7;
    
        DestCCB = CurrentCCB;       // Copy pointer to local 
        if (DestCCB>=&gCCBArray[CCBTotal]) {     // Am I full already? 
            FlushCCBs();                // Draw all the CCBs/Lines 
            DestCCB = gCCBArray;
        }

        Colnum7 = Colnum & 7;   // Get the pixel skip 
        Colnum = Colnum>>1;     // Pixel to byte offset 
        Colnum += 32;           // Index past the PLUT 
        Colnum &= ~3;           // Long word align the source 
        DestCCB[0].ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT|CCB_USEAV;  // ccb_flags 
        DestCCB[0].ccb_PRE0 = (Colnum7<<24)|0x03;
        DestCCB[0].ccb_PRE1 = 0x3E005000|(Colnum7+Run-1);   // Project the pixels 
        DestCCB[0].ccb_PLUTPtr = Source;        // Get the palette ptr 
        DestCCB[0].ccb_SourcePtr = (CelData *)&Source[Colnum];  // Get the source ptr 
        DestCCB[0].ccb_XPos = gTexX<<16;     // Set the x and y coord for start 
        DestCCB[0].ccb_YPos = (y<<16)+0xFF00;
        DestCCB[0].ccb_HDX = 0<<20;     // Convert 6 bit frac to CCB scale 
        DestCCB[0].ccb_HDY = (gTexScale<<11);
        DestCCB[0].ccb_VDX = 1<<16;
        DestCCB[0].ccb_VDY = 0<<16;
        DestCCB[0].ccb_PIXC = gLightTable[tx_texturelight>>LIGHTSCALESHIFT];     // PIXC control 
    
        ++DestCCB;              // Next CCB 
        CurrentCCB = DestCCB;   // Save the CCB pointer 
    #endif
}

/**********************************

    Drawing the sky is the easiest, so I'll do this first.
    The parms are, 
    gTexX = screen x coord for the virtual screen.
    colnum = index for which scan line to draw from the source image. Note that
        this number has all bits set so I must mask off the unneeded bits for
        texture wraparound.
        
    No light shading is used for the sky. The scale factor is a constant.
    
**********************************/

extern uint32_t gTexX;
extern int gTexScale;

static void DrawSkyLine()
{
    // Note: sky textures are 256 pixels wide so this wraps around
    const uint32_t colNum = (((gXToViewAngle[gTexX] + gViewAngle) >> ANGLETOSKYSHIFT) & 0xFF);
    
    // Sky is always rendered at max light and 1.0 scale
    gTxTextureLight = MAX_WALL_LIGHT_VALUE << LIGHTSCALESHIFT;
    gTexScale = 1 << SCALEBITS;
    
    // Set source texture details
    // TODO: don't keep doing this for each column
    const Texture* const pTexture = (const Texture*) getWallTexture(getCurrentSkyTexNum());
    const uint32_t texHeight = pTexture->height;
    DrawWallColumn(0, colNum * texHeight, 0, texHeight, pTexture->pData, texHeight);
    
    // DC: FIXME: implement/replace
    #if 0
        Byte *Source;
        Word Colnum;
        MyCCB* DestCCB;         // Pointer to new CCB entry 
    
        DestCCB = CurrentCCB;       // Copy pointer to local 
        if (DestCCB>=&gCCBArray[CCBTotal]) {     // Am I full already? 
            FlushCCBs();                // Draw all the CCBs/Lines 
            DestCCB = gCCBArray;
        }
        Colnum = (((xtoviewangle[gTexX]+viewangle)>>ANGLETOSKYSHIFT)&0xFF)*64;
        Source = (Byte *)(*SkyTexture->data);   // Index to the true shape 

        DestCCB[0].ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT;    // ccb_flags 
        DestCCB[0].ccb_PRE0 = 0x03;
        DestCCB[0].ccb_PRE1 = 0x3E005000|(128-1);   // Project the pixels 
        DestCCB[0].ccb_PLUTPtr = Source;        // Get the palette ptr 
        DestCCB[0].ccb_SourcePtr = (CelData *)&Source[Colnum+32];   // Get the source ptr 
        DestCCB[0].ccb_XPos = gTexX<<16;     // Set the x and y coord for start 
        DestCCB[0].ccb_YPos = 0<<16;
        DestCCB[0].ccb_HDX = 0<<20;     // Convert 6 bit frac to CCB scale 
        DestCCB[0].ccb_HDY = gSkyScales[ScreenSize]; // Video stretch factor 
        DestCCB[0].ccb_VDX = 1<<16;
        DestCCB[0].ccb_VDY = 0<<16;
        DestCCB[0].ccb_PIXC = 0x1F00;       // PIXC control 
        ++DestCCB;          // Next CCB 
        CurrentCCB = DestCCB;   // Save the CCB pointer 
    #endif
}

/**********************************

    Calculate texturecolumn and iscale for the rendertexture routine

**********************************/

static void DrawTexture(drawtex_t *tex)
{
    int top;
    uint32_t Run;
    uint32_t colnum;    // Column in the texture
    uint32_t frac;

    Run = (tex->topheight-tex->bottomheight)>>HEIGHTBITS;   // Source image height
    if ((int)Run<=0) {      // Invalid?
        return;
    }
    top = gCenterY-((gTexScale*tex->topheight)>>(HEIGHTBITS+SCALEBITS));  // Screen Y

    colnum = gTexTextureColumn;  // Get the starting column offset
    frac = tex->texturemid - (tex->topheight<<FIXEDTOHEIGHT);   // Get the anchor point
    frac >>= FRACBITS;
    while (frac&0x8000) {
        --colnum;
        frac += tex->height;        // Make sure it's on the shape
    }
    frac&=0x7f;                         // Zap unneeded bits
    colnum &= (tex->width-1);           // Wrap around the texture
    colnum = (colnum * tex->height);    // Index to the shape

    // Project it
    DrawWallColumn(
        top,
        colnum,
        frac,
        tex->height,
        tex->data,
        Run
    );   
}

/**********************************

    Draw a single wall texture.
    Also save states for pending ceiling, floor and future clipping

**********************************/

static void DrawSeg(viswall_t *segl)
{
    uint32_t x;        // Current x coord
    int scale;
    int _scalefrac;
    uint32_t ActionBits;
    ActionBits = segl->WallActions;
    
    if (ActionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE)) {
        x = segl->seglightlevel;
        gLightMin = gLightMins[x];
        gLightMax = x;
        gLightSub = gLightSubs[x];
        gLightCoef = gLightCoefs[x];
        
        if (ActionBits & AC_TOPTEXTURE) {   // Is there a top wall?
            gTopTex.topheight = segl->t_topheight;   // Init the top texture
            gTopTex.bottomheight = segl->t_bottomheight;
            gTopTex.texturemid = segl->t_texturemid;
            
            const Texture* const pTex = segl->t_texture;
            gTopTex.width = pTex->width;
            gTopTex.height = pTex->height;
            gTopTex.data = pTex->pData;
        }
        
        if (ActionBits & AC_BOTTOMTEXTURE) {  // Is there a bottom wall?
            gBottomTex.topheight = segl->b_topheight;
            gBottomTex.bottomheight = segl->b_bottomheight;
            gBottomTex.texturemid = segl->b_texturemid;
            
            const Texture* const pTex = segl->b_texture;
            gBottomTex.width = pTex->width;
            gBottomTex.height = pTex->height;
            gBottomTex.data = pTex->pData;
        }
        
        _scalefrac = segl->LeftScale;   // Init the scale fraction
        x = segl->LeftX;                // Init the x coord
        
        do {                                        // Loop for each X coord
            scale = _scalefrac >> FIXEDTOSCALE;     // Current scaling factor
            
            if (scale >= 0x2000) {                  // Too large?
                scale = 0x1fff;                     // Fix the scale to maximum
            }
            
            gTexX = x;   // Pass the X coord

            // Calculate texture offset into shape
            gTexTextureColumn = (
                segl->offset - fixedMul(
                    gFineTangent[(segl->CenterAngle + gXToViewAngle[x]) >> ANGLETOFINESHIFT],
                    segl->distance
                )
            ) >> FRACBITS;
            
            gTexScale = scale;   // 0-0x1FFF
            
            {
                int texturelight = ((scale * gLightCoef) >> 16) - gLightSub;
                
                if (texturelight < gLightMin) {
                    texturelight = gLightMin;
                }
                
                if (texturelight > gLightMax) {
                    texturelight = gLightMax;
                }
                
                gTxTextureLight = texturelight;
            }
            
            if (ActionBits&AC_TOPTEXTURE) {
                DrawTexture(&gTopTex);          // Draw upper texture
            }
            
            if (ActionBits&AC_BOTTOMTEXTURE) {
                DrawTexture(&gBottomTex);       // Draw lower texture
            }
            
            _scalefrac += segl->ScaleStep;      // Step to the next scale
            
        } while (++x <= segl->RightX);
    }
}

/**********************************

    Given a span of pixels, see if it is already defined
    in a record somewhere. If it is, then merge it otherwise
    make a new plane definition.

**********************************/

static visplane_t* FindPlane(visplane_t *check, Fixed height, uint32_t PicHandle, int start, int stop, uint32_t Light)
{
    uint32_t i;
    uint32_t j;
    uint32_t *set;

    ++check;        // Automatically skip to the next plane
    if (check<gpEndVisPlane) {
        do {
            if (height == check->height &&      // Same plane as before?
                PicHandle == check->PicHandle &&
                Light == check->PlaneLight &&
                check->open[start] == OPENMARK) {   // Not defined yet?
                if (start < check->minx) {  // In range of the plane?
                    check->minx = start;    // Mark the new edge
                }
                if (stop > check->maxx) {
                    check->maxx = stop;     // Mark the new edge
                }
                return check;           // Use the same one as before
            }
        } while (++check<gpEndVisPlane);
    }
    
// make a new plane
    
    check = gpEndVisPlane;
    ++gpEndVisPlane;
    check->height = height;     // Init all the vars in the visplane
    check->PicHandle = PicHandle;
    check->minx = start;
    check->maxx = stop;
    check->PlaneLight = Light;      // Set the light level

// Quickly fill in the visplane table

    i = OPENMARK;
    set = check->open;  // A brute force method to fill in the visplane record FAST!
    j = gScreenWidth/8;
    do {
        set[0] = i;
        set[1] = i;
        set[2] = i;
        set[3] = i;
        set[4] = i;
        set[5] = i;
        set[6] = i;
        set[7] = i;
        set+=8;
    } while (--j);
    return check;
}


/**********************************

    Do a fake wall rendering so I can get all the visplane records.
    This is a fake-o routine so I can later draw the wall segments from back to front.

**********************************/

static void SegLoop(viswall_t *segl)
{
    uint32_t x;        // Current x coord
    int scale;
    int _scalefrac;
    uint32_t ActionBits;
    visplane_t *FloorPlane,*CeilingPlane;
    int ceilingclipy,floorclipy;
    
    _scalefrac = segl->LeftScale;       // Init the scale fraction

            // visplanes[0] is zero to force a FindPlane on the first pass
            
    FloorPlane = CeilingPlane = gVisPlanes;      // Reset the visplane pointers
    ActionBits = segl->WallActions;
    x = segl->LeftX;                // Init the x coord
    do {                            // Loop for each X coord
        scale = _scalefrac>>FIXEDTOSCALE;   // Current scaling factor
        if (scale >= 0x2000) {      // Too large?
            scale = 0x1fff;         // Fix the scale to maximum
        }
        ceilingclipy = gClipBoundTop[x];    // Get the top y clip
        floorclipy = gClipBoundBottom[x];   // Get the bottom y clip

// Shall I add the floor?

        if (ActionBits & AC_ADDFLOOR) {
            int top,bottom;
            top = gCenterY-((scale*segl->floorheight)>>(HEIGHTBITS+SCALEBITS));  // Y coord of top of floor
            if (top <= ceilingclipy) {
                top = ceilingclipy+1;       // Clip the top of floor to the bottom of the visible area
            }
            bottom = floorclipy-1;      // Draw to the bottom of the screen
            if (top <= bottom) {        // Valid span?
                if (FloorPlane->open[x] != OPENMARK) {  // Not already covered?
                    FloorPlane = FindPlane(FloorPlane,segl->floorheight,
                        segl->FloorPic,x,segl->RightX,segl->seglightlevel);
                }
                if (top) {
                    --top;
                }
                FloorPlane->open[x] = (top<<8)+bottom;  // Set the new vertical span
            }
        }

// Handle ceilings

        if (ActionBits & AC_ADDCEILING) {
            int top,bottom;
            top = ceilingclipy+1;       // Start from the ceiling
            bottom = gCenterY-1-((scale*segl->ceilingheight)>>(HEIGHTBITS+SCALEBITS));   // Bottom of the height
            if (bottom >= floorclipy) {     // Clip the bottom?
                bottom = floorclipy-1;
            }
            if (top <= bottom) {
                if (CeilingPlane->open[x] != OPENMARK) {        // Already in use?
                    CeilingPlane = FindPlane(CeilingPlane,segl->ceilingheight,
                        segl->CeilingPic,x,segl->RightX,segl->seglightlevel);
                }
                if (top) {
                    --top;
                }
                CeilingPlane->open[x] = (top<<8)+bottom;        // Set the vertical span
            }
        }

// Sprite clip sils

        if (ActionBits & (AC_BOTTOMSIL|AC_NEWFLOOR)) {
            int low;
            low = gCenterY-((scale*segl->floornewheight)>>(HEIGHTBITS+SCALEBITS));
            if (low > floorclipy) {
                low = floorclipy;
            }
            if (low < 0) {
                low = 0;
            }
            if (ActionBits & AC_BOTTOMSIL) {
                segl->BottomSil[x] = low;
            } 
            if (ActionBits & AC_NEWFLOOR) {
                gClipBoundBottom[x] = low;
            }
        }

        if (ActionBits & (AC_TOPSIL|AC_NEWCEILING)) {
            int high;
            high = (gCenterY-1)-((scale*segl->ceilingnewheight)>>(HEIGHTBITS+SCALEBITS));
            if (high < ceilingclipy) {
                high = ceilingclipy;
            }
            if (high > (int)gScreenHeight-1) {
                high = gScreenHeight-1;
            }
            if (ActionBits & AC_TOPSIL) {
                segl->TopSil[x] = high+1;
            }
            if (ActionBits & AC_NEWCEILING) {
                gClipBoundTop[x] = high;
            }
        }

// I can draw the sky right now!!

        if (ActionBits & AC_ADDSKY) {
            int bottom;
            bottom = gCenterY-((scale*segl->ceilingheight)>>(HEIGHTBITS+SCALEBITS));
            if (bottom > floorclipy) {
                bottom = floorclipy;
            }
            if ((ceilingclipy+1) < bottom) {        // Valid?
                gTexX = x;          // Pass the X coord
                DrawSkyLine();      // Draw the sky
            }
        }
        _scalefrac += segl->ScaleStep;      // Step to the next scale
    } while (++x<=segl->RightX);
}

/**********************************

    Follow the list of walls and draw each
    and every wall fragment.
    Note : I draw the walls closest to farthest and I maintain a ZBuffet

**********************************/

void SegCommands(void)
{
    {
        uint32_t i;     // Temp index
        viswall_t *WallSegPtr;      // Pointer to the current wall
        viswall_t *LastSegPtr;
    
        WallSegPtr = gVisWalls;             // Get the first wall segment to process
        LastSegPtr = gpEndVisWall;          // Last one to process

        if (LastSegPtr == WallSegPtr) {     // No walls to render?
            return;                         // Exit now!!
        }

        EnableHardwareClipping();       // Turn on all hardware clipping to remove slop
    
        i = 0;      // Init the vertical clipping records
        do {
            gClipBoundTop[i] = -1;       // Allow to the ceiling
            gClipBoundBottom[i] = gScreenHeight;  // Stop at the floor
        } while (++i<gScreenWidth);

        // Process all the wall segments

        do {
            SegLoop(WallSegPtr);            // Create the viswall records and draw the sky only
        } while (++WallSegPtr<LastSegPtr);  // Next wall in chain
    
        // Now I actually draw the walls back to front to allow for clipping because of slop
    
        LastSegPtr = gVisWalls;      // Stop at the last one
        do {
            --WallSegPtr;           // Last go backwards!!
            DrawSeg(WallSegPtr);        // Draw the wall (Only if needed)
        } while (WallSegPtr!=LastSegPtr);   // All done?
    }
}

END_NAMESPACE(Renderer)
