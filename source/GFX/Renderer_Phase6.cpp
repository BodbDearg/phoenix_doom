#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Textures.h"
#include "ThreeDO.h"
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
    if (check<gLastVisPlane) {
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
        } while (++check<gLastVisPlane);
    }
    
// make a new plane
    
    check = gLastVisPlane;
    ++gLastVisPlane;
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

    Draw all the sprites from back to front.
    
**********************************/

static void DrawSprites(void)
{
    uint32_t i;
    uint32_t *LocalPtr;
    vissprite_t *VisPtr;
    
    i = gSpriteTotal;    // Init the count
    if (i) {        // Any sprites to speak of?
        LocalPtr = gSortedSprites;   // Get the pointer to the sorted array
        VisPtr = gVisSprites;    // Cache pointer to sprite array
        do {
            DrawVisSprite(&VisPtr[*LocalPtr++&0x7F]);   // Draw from back to front
        } while (--i);
    }
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
    
    
        WallSegPtr = gVisWalls;      // Get the first wall segment to process
        LastSegPtr = gLastWallCmd;   // Last one to process
        if (LastSegPtr == WallSegPtr) { // No walls to render?
            return;             // Exit now!!
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

    // Now we draw all the planes. They are already clipped and create no slop!
    {   
        visplane_t *PlanePtr;
        visplane_t *LastPlanePtr;
        uint32_t WallScale;
        
        PlanePtr = gVisPlanes+1;     // Get the range of pointers
        LastPlanePtr = gLastVisPlane;
    
        if (PlanePtr!=LastPlanePtr) {   // No planes generated?
            gPlaneY = -gViewY;        // Get the Y coord for camera
            WallScale = (gViewAngle-ANG90)>>ANGLETOFINESHIFT;    // left to right mapping
            gBaseXScale = (gFineCosine[WallScale] / ((int)gScreenWidth/2));
            gBaseYScale = -(gFineSine[WallScale] / ((int)gScreenWidth/2));
            do {
                DrawVisPlane(PlanePtr);     // Convert the plane
            } while (++PlanePtr<LastPlanePtr);      // Loop for all
        }
    }

    DisableHardwareClipping();      // Sprites require full screen management
    DrawSprites();                  // Draw all the sprites (ZSorted and clipped)
}

END_NAMESPACE(Renderer)
