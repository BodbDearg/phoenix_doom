#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Textures.h"
#include "ThreeDO.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

#define OPENMARK ((MAXSCREENHEIGHT-1)<<8)

std::byte*  gPlaneSource;
Fixed       gPlaneY;
Fixed       gBaseXScale;
Fixed       gBaseYScale;
uint32_t    gPlaneDistance;

static uint32_t gPlaneHeight;
static uint32_t gSpanStart[MAXSCREENHEIGHT];

/**********************************

    Drawing the floor and ceiling is the hardest, so I'll do this last.
    The parms are, 
    x = screen x coord for the virtual screen. Must be offset
        to the true screen coords.
    top = screen y coord for the virtual screen. Must also be offset
        to the true screen coords.
    bottom = screen y coord for the BOTTOM of the pixel run. Subtract from top
        to get the exact destination pixel run count.
    colnum = index for which scan line to draw from the source image. Note that
        this number has all bits set so I must mask off the unneeded bits for
        texture wraparound.

    gSpanPtr is a pointer to the gSpanArray buffer, this is where I store the
    processed floor textures.
    No light shading is used. The scale factor is a constant.
    
**********************************/
static void DrawFloorColumn(
    uint32_t ds_y,
    uint32_t ds_x1,
    uint32_t Count,
    uint32_t xfrac,
    uint32_t yfrac,
    Fixed ds_xstep,
    Fixed ds_ystep
) {
    // TODO: TEMP - CLEANUP
    const uint16_t* const pPLUT = (const uint16_t*) gPlaneSource;
    const Fixed lightMultiplier = getLightMultiplier(gTxTextureLight, MAX_FLOOR_LIGHT_VALUE);

    for (uint32_t pixelNum = 0; pixelNum < Count; ++pixelNum) {
        Fixed tx = ((xfrac + ds_xstep * pixelNum) >> FRACBITS) & 63;    // assumes 64x64
        Fixed ty = ((yfrac + ds_ystep * pixelNum) >> FRACBITS) & 63;    // assumes 64x64

        Fixed offset = ty * 64 + tx;
        const uint8_t lutByte = ((uint8_t) gPlaneSource[64 + offset]) & 31;
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

    // DC: FIXME: implement/replace
    #if 0
        Byte *DestPtr;
        MyCCB *DestCCB;

        DestCCB = CurrentCCB;       // Copy pointer to local 
        if (DestCCB>=&gCCBArray[CCBTotal]) {     // Am I full already? 
            FlushCCBs();                // Draw all the CCBs/Lines 
            DestCCB=gCCBArray;
        }
        DestPtr = gSpanPtr;
        DrawASpan(Count,xfrac,yfrac,ds_xstep,ds_ystep,DestPtr);

        DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT|CCB_USEAV;  // ccb_flags 

        DestCCB->ccb_PRE0 = 0x00000005;     // Preamble (Coded 8 bit) 
        DestCCB->ccb_PRE1 = 0x3E005000|(Count-1);       // Second preamble 
        DestCCB->ccb_SourcePtr = (CelData *)DestPtr;    // Save the source ptr 
        DestCCB->ccb_PLUTPtr = PlaneSource;     // Get the palette ptr 
        DestCCB->ccb_XPos = ds_x1<<16;      // Set the x and y coord for start 
        DestCCB->ccb_YPos = ds_y<<16;
        DestCCB->ccb_HDX = 1<<20;       // OK 
        DestCCB->ccb_HDY = 0<<20;
        DestCCB->ccb_VDX = 0<<16;
        DestCCB->ccb_VDY = 1<<16;
        DestCCB->ccb_PIXC = gLightTable[tx_texturelight>>LIGHTSCALESHIFT];           // PIXC control 
    
        Count = (Count+3)&(~3);     // Round to nearest longword 
        DestPtr += Count;
        gSpanPtr = DestPtr;
        ++DestCCB;              // Next CCB 
        CurrentCCB = DestCCB;   // Save the CCB pointer 
    #endif
}

/**********************************

basexscale
baseyscale
planey

    This is the basic primitive to draw horizontal lines quickly
    
**********************************/
static void MapPlane(uint32_t x2, uint32_t y)
{
    angle_t angle;
    uint32_t distance;
    Fixed length;
    Fixed xfrac,yfrac,xstep,ystep;
    uint32_t x1;

// planeheight is 10.6
// yslope is 6.10, distscale is 1.15
// distance is 12.4
// length is 11.5

    x1 = gSpanStart[y];
    distance = (gYSlope[y]*gPlaneHeight)>>12; // Get the offset for the plane height
    length = (gDistScale[x1]*distance)>>14;
    angle = (gXToViewAngle[x1]+gViewAngle)>>ANGLETOFINESHIFT;

// xfrac, yfrac, xstep, ystep

    xfrac = (((gFineCosine[angle]>>1)*length)>>4)+gViewX;
    yfrac = gPlaneY - (((gFineSine[angle]>>1)*length)>>4);

    xstep = ((Fixed)distance*gBaseXScale)>>4;
    ystep = ((Fixed)distance*gBaseYScale)>>4;

    distance = (distance > 0) ? distance : 1;               // DC: fix division by zero when using the noclip cheat
    length = gLightCoef / (Fixed) distance - gLightSub;

    if (length < gLightMin) {
        length = gLightMin;
    }
    
    if (length > gLightMax) {
        length = gLightMax;
    }

    gTxTextureLight = length;
    DrawFloorColumn(y,x1,x2-x1,xfrac,yfrac,xstep,ystep);
}

/**********************************
    
    Draw a plane by scanning the open records. 
    The open records are an array of top and bottom Y's for
    a graphical plane. I traverse the array to find out the horizontal
    spans I need to draw. This is a bottleneck routine.

**********************************/

void DrawVisPlane(visplane_t *p)
{
    uint32_t x;
    uint32_t stop;
    uint32_t oldtop;
    uint32_t *open;

    const Texture* const pTex = getFlatAnimTexture((uint32_t) p->PicHandle);
    gPlaneSource = (std::byte*) pTex->pData;
    x = p->height;
    if ((int)x<0) {
        x = -x;
    }
    gPlaneHeight = x;
    
    stop = p->PlaneLight;
    gLightMin = gLightMins[stop];
    gLightMax = stop;
    gLightSub = gLightSubs[stop];
    gLightCoef = gPlaneLightCoef[stop];
    
    stop = p->maxx+1;   // Maximum x coord
    x = p->minx;        // Starting x
    open = p->open;     // Init the pointer to the open Y's
    oldtop = OPENMARK;  // Get the top and bottom Y's
    open[stop] = oldtop;    // Set posts to stop drawing

    do {
        uint32_t newtop;
        newtop = open[x];       // Fetch the NEW top and bottom
        if (oldtop!=newtop) {
            uint32_t PrevTopY,NewTopY;      // Previous and dest Y coords for top line
            uint32_t PrevBottomY,NewBottomY;    // Previous and dest Y coords for bottom line
            PrevTopY = oldtop>>8;       // Starting Y coords
            PrevBottomY = oldtop&0xFF;
            NewTopY = newtop>>8;
            NewBottomY = newtop&0xff;
        
            // For lines on the top, check if the entry is going down
            
            if (PrevTopY < NewTopY && PrevTopY<=PrevBottomY) {  // Valid?
                uint32_t Count;
                    
                Count = PrevBottomY+1;  // Convert to <
                if (NewTopY<Count) {    // Use the lower
                    Count = NewTopY;    // This is smaller
                }
                do {
                    MapPlane(x,PrevTopY);       // Draw to this x
                } while (++PrevTopY<Count); // Keep counting
            }
            if (NewTopY < PrevTopY && NewTopY<=NewBottomY) {
                uint32_t Count;
                Count = NewBottomY+1;
                if (PrevTopY<Count) {
                    Count = PrevTopY;
                }
                do {
                    gSpanStart[NewTopY] = x; // Mark the starting x's
                } while (++NewTopY<Count);
            }
        
            if (PrevBottomY > NewBottomY && PrevBottomY>=PrevTopY) {
                int Count;
                Count = PrevTopY-1;
                if (Count<(int)NewBottomY) {
                    Count = NewBottomY;
                }
                do {
                    MapPlane(x,PrevBottomY);    // Draw to this x
                } while ((int)--PrevBottomY>Count);
            }
            if (NewBottomY > PrevBottomY && NewBottomY>=NewTopY) {
                int Count;
                Count = NewTopY-1;
                if (Count<(int)PrevBottomY) {
                    Count = PrevBottomY;
                }
                do {
                    gSpanStart[NewBottomY] = x;      // Mark the starting x's
                } while ((int)--NewBottomY>Count);
            }
            oldtop=newtop;
        }
    } while (++x<=stop);
}

void drawAllVisPlanes() noexcept {
    visplane_t *PlanePtr;
    visplane_t *LastPlanePtr;
    uint32_t WallScale;
        
    PlanePtr = gVisPlanes+1;     // Get the range of pointers
    LastPlanePtr = gpEndVisPlane;
    
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

END_NAMESPACE(Renderer)
