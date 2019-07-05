#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "Map/MapData.h"
#include "Sprites.h"
#include "Things/Info.h"
#include "Things/MapObj.h"
#include "Video.h"
#include <algorithm>

BEGIN_NAMESPACE(Renderer)

// TODO: CLEANUP
/*
#if 0
    0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,    // 1/16 - 8/16 
    0x00D0,0x1300,0x08D0,0x1700,0x10D0,0x1B00,0x18D0,0x1F00,
#endif

static uint32_t gLightTable[] = {
    0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,    // 1/16 - 8/16 
    0x00D0,0x00D0,0x1300,0x1300,0x08D0,0x08D0,0x1700,0x1700,
    0x10D0,0x10D0,0x1B00,0x1B00,0x18D0,0x18D0,0x1F00,0x1F00,
    0x1F00,0x1F00,0x1F00,0x1F00,0x1F00,0x1F00,0x1F00,0x1F00,
};
*/

static std::byte* gSpritePLUT;
static uint32_t gSpriteY;
static uint32_t gSpriteYScale;
/*
static uint32_t gSpritePIXC;
static uint32_t gSpritePRE0;
static uint32_t gSpritePRE1;
*/
static std::byte* gStartLinePtr;
static uint32_t gSpriteWidth;

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
// Utility that determines how much to step (in texels) per pixel to render the entire of the given
// sprite dimension in the given render area dimension (both in pixels).
//----------------------------------------------------------------------------------------------------------------------
static Fixed determineTexelStep(const uint32_t textureSize, const uint32_t renderSize) {
    if (textureSize <= 1 || renderSize <= 1) {
        return 0;
    }
    
    // The way the math works here helps ensure the last pixel drawn is pretty much always the last pixel
    // of the texture. This ensures that edges/borders around sprites etc. don't seem to vanish... 
    // I used to have issues with the bottom rows of the explosive barrels cutting out before adopting this method.
    const int32_t numPixelSteps = (int32_t) renderSize - 1;
    const Fixed step = fixedDiv(
        intToFixed(textureSize) - 1,    // N.B: never let it reach 'textureSize' - keep below, as that is an out of bounds index!
        intToFixed(numPixelSteps)
    );

    return step;
}

//----------------------------------------------------------------------------------------------------------------------
// This routine will draw a scaled sprite during the game. It is called when there is no 
// onscreen clipping needed or if the only clipping is to the screen bounds.
//----------------------------------------------------------------------------------------------------------------------
static void drawSpriteNoClip(const vissprite_t& visSprite) noexcept {
    // Get the left, right, top and bottom screen edges for the sprite to be rendered.
    // Also check if the sprite is completely offscreen, because the input is not clipped.
    // 3DO Doom originally relied on the hardware to clip in this routine...
    int x1 = visSprite.x1;
    int x2 = visSprite.x2;
    int y1 = visSprite.y1;
    int y2 = visSprite.y2;

    const bool bCompletelyOffscreen = (
        (x1 >= (int) gScreenWidth) ||
        (x2 < 0) ||
        (y1 >= (int) gScreenHeight) ||
        (y2 < 0)
    );

    if (bCompletelyOffscreen)
        return;

    // Get the light multiplier to use for lighting the sprite
    const Fixed lightMultiplier = getLightMultiplier(visSprite.colormap & 0xFF, MAX_SPRITE_LIGHT_VALUE);
    
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
    const Fixed texelStepX_NoFlip = determineTexelStep(spriteW, renderW);
    const Fixed texelStepY = determineTexelStep(spriteH, renderH);

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
        const int pixelsOffscreen = -x1;
        startTexelX += pixelsOffscreen * texelStepX;
        x1 = 0;
    }

    if (x2 >= (int) gScreenWidth) {
        x2 = gScreenWidth - 1;
    }
    
    if (y1 < 0) {
        const int pixelsOffscreen = -y1;
        startTexelY += pixelsOffscreen * texelStepY;
        y1 = 0;
    }

    if (y2 >= (int) gScreenHeight) {
        y2 = gScreenHeight - 1;
    }
    
    // Render all the columns of the sprite
    const uint16_t* const pImage = pSpriteFrame->pTexture;
    Fixed texelXFrac = startTexelX;

    for (int x = x1; x <= x2; ++x) {
        const int texelXInt = fixedToInt(texelXFrac);
        Fixed texelYFrac = startTexelY;
        
        const uint16_t* const pImageCol = pImage + texelXInt * spriteH;
        uint32_t* pDstPixel = &Video::gFrameBuffer[x + gScreenXOffset + (y1 + gScreenYOffset) * Video::SCREEN_WIDTH];

        for (int y = y1; y <= y2; ++y) {
            // Grab this pixels color from the sprite image and skip if alpha 0
            const int texelYInt = fixedToInt(texelYFrac);

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

                const Fixed darkenedR = fixedMul(texRFrac, lightMultiplier);
                const Fixed darkenedG = fixedMul(texGFrac, lightMultiplier);
                const Fixed darkenedB = fixedMul(texBFrac, lightMultiplier);

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

    /*
    patch_t *patch;     // Pointer to the actual sprite record
    Word ColorMap;
    int x;
    
    patch = (patch_t *)loadResourceData(vis->PatchLump);   
    patch =(patch_t *) &((Byte *)patch)[vis->PatchOffset];

    ((LongWord *)patch)[7] = 0;
    ((LongWord *)patch)[10] = 0;
    ((LongWord *)patch)[8] = vis->yscale<<4;
    ColorMap = vis->colormap;
    if (ColorMap&0x8000) {
        ((LongWord *)patch)[13] = 0x9C81;
    } else {
        ((LongWord *)patch)[13] = gLightTable[(ColorMap&0xFF)>>LIGHTSCALESHIFT];
    }
    if (ColorMap&0x4000) {
        x = vis->x2;
        ((LongWord *)patch)[9] = -vis->xscale;
    } else {
        x = vis->x1;
        ((LongWord *)patch)[9] = vis->xscale;
    }
    DrawMShape(x+ScreenXOffset,vis->y1+ScreenYOffset,&patch->Data);
    releaseResource(vis->PatchLump);
    */
}

//----------------------------------------------------------------------------------------------------------------------
// Draws a column of a sprite that is clipped to the given top and bottom bounds
//----------------------------------------------------------------------------------------------------------------------
static void oneSpriteLine(
    const uint32_t screenX,
    const Fixed spriteX,
    const uint32_t topClipY,
    const uint32_t bottomClipY,
    const vissprite_t& visSprite
) noexcept {
    // Sanity checks
    ASSERT(topClipY >= 0 && topClipY <= gScreenHeight);
    ASSERT(bottomClipY >= 0 && bottomClipY <= gScreenHeight);

    // Get the top and bottom screen edges for the sprite columnn to be rendered.
    // Also check if the column is completely offscreen, because the input is not clipped.
    // 3DO Doom originally relied on the hardware to clip in this routine...
    if (screenX < 0 || screenX >= (int) gScreenWidth)
        return;
    
    int y1 = visSprite.y1;
    int y2 = visSprite.y2;
    
    if (y1 >= (int) gScreenHeight || y2 < 0)
        return;
    
    // If the clip bounds are meeting (or past each other?!) then ignore
    if (topClipY >= bottomClipY)
        return;
    
    // Get the light multiplier to use for lighting the sprite
    const Fixed lightMultiplier = getLightMultiplier(visSprite.colormap & 0xFF, MAX_SPRITE_LIGHT_VALUE);
    
    // Get the height of the sprite and also the height that it will be rendered at.
    // Note that we expect no zero sizes here!
    const SpriteFrameAngle* const pSpriteFrame = visSprite.pSprite;
    
    const int32_t spriteH = pSpriteFrame->height;
    const uint32_t renderH = (visSprite.y2 - visSprite.y1) + 1;
    ASSERT(spriteH > 0);
    ASSERT(renderH > 0);
    
    // Figure out the step in texels we want per y pixel in 16.16 format
    const Fixed texelStepY = determineTexelStep(spriteH, renderH);

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
    
    if (y1 < (int) topClipY) {
        const int pixelsOffscreen = topClipY - y1;
        startTexelY += pixelsOffscreen * texelStepY;
        y1 = topClipY;
    }
    
    if (y2 >= (int) bottomClipY) {
        y2 = bottomClipY - 1;
    }
    
    // Render the sprite column
    const uint16_t* const pImageCol = getSpriteColumn(visSprite, spriteX);
    Fixed texelYFrac = startTexelY;
    uint32_t* pDstPixel = &Video::gFrameBuffer[screenX + gScreenXOffset + (y1 + gScreenYOffset) * Video::SCREEN_WIDTH];

    for (int y = y1; y <= y2; ++y) {
        // Grab this pixels color from the sprite image and skip if alpha 0
        const int texelYInt = fixedToInt(texelYFrac);
        
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

            const Fixed darkenedR = fixedMul(texRFrac, lightMultiplier);
            const Fixed darkenedG = fixedMul(texGFrac, lightMultiplier);
            const Fixed darkenedB = fixedMul(texBFrac, lightMultiplier);
            
            // Save the final output color
            const uint32_t finalColor = Video::fixedRgbToScreenCol(darkenedR, darkenedG, darkenedB);
            *pDstPixel = finalColor;
        }
        
        // Onto the next pixel in the column
        texelYFrac += texelStepY;
        pDstPixel += Video::SCREEN_WIDTH;
    }

    // DC: FIXME: implement/replace
    #if 0
        MyCCB *DestCCB;

        DestCCB = CurrentCCB;       // Copy pointer to local 
        if (DestCCB>=&gCCBArray[CCBTotal]) {     // Am I full already? 
            FlushCCBs();                // Draw all the CCBs/Lines 
            DestCCB=gCCBArray;
        }
        DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_PACKED|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT;    // ccb_flags 

        DestCCB->ccb_PIXC = gSpritePIXC;         // PIXC control 
        DestCCB->ccb_PRE0 = gSpritePRE0;     // Preamble (Coded 8 bit) 
        DestCCB->ccb_PRE1 = gSpritePRE1;     // Second preamble 
        DestCCB->ccb_SourcePtr = (CelData *)SpriteLinePtr;  // Save the source ptr 
        DestCCB->ccb_PLUTPtr = gSpritePLUT;      // Get the palette ptr 
        DestCCB->ccb_XPos = (x1+ScreenXOffset)<<16;     // Set the x and y coord for start 
        DestCCB->ccb_YPos = gSpriteY;
        DestCCB->ccb_HDX = 0<<20;       // OK 
        DestCCB->ccb_HDY = gSpriteYScale;
        DestCCB->ccb_VDX = 1<<16;
        DestCCB->ccb_VDY = 0<<16;
        ++DestCCB;          // Next CCB 
        CurrentCCB = DestCCB;   // Save the CCB pointer 
    #endif
}

// DC: FIXME: implement/replace
#if 0
/**********************************

    This routine will draw a scaled sprite during the game.
    It is called when there is onscreen clipping needed so I
    use the global table spropening to get the top and bottom clip
    bounds.
 
    I am passed the screen clipped x1 and x2 coords.

**********************************/
static void OneSpriteClipLine(Word x1,Byte *SpriteLinePtr,int Clip,int Run)
{
    MyCCB *DestCCB;

    DrawARect(0,191,Run,1,BLACK);
    DestCCB = CurrentCCB;       // Copy pointer to local 
    if (DestCCB>=&gCCBArray[CCBTotal-1]) {       // Am I full already? 
        FlushCCBs();                // Draw all the CCBs/Lines 
        DestCCB=gCCBArray;
    }
    DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_PACKED|
    CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
    CCB_ACE|CCB_BGND|CCB_PPABS|CCB_LDPLUT;  // ccb_flags 

    DestCCB->ccb_PIXC = 0x1F00;         // PIXC control 
    DestCCB->ccb_PRE0 = gSpritePRE0;     // Preamble (Coded 8 bit) 
    DestCCB->ccb_PRE1 = gSpritePRE1;     // Second preamble 
    DestCCB->ccb_SourcePtr = (CelData *)SpriteLinePtr;  // Save the source ptr 
    DestCCB->ccb_PLUTPtr = gSpritePLUT;      // Get the palette ptr 
    DestCCB->ccb_XPos = -(Clip<<16);        // Set the x and y coord for start 
    DestCCB->ccb_YPos = 191<<16;
    DestCCB->ccb_HDX = gSpriteYScale;        // OK 
    DestCCB->ccb_HDY = 0<<20;
    DestCCB->ccb_VDX = 0<<16;
    DestCCB->ccb_VDY = 1<<16;
    ++DestCCB;          // Next CCB 

    DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
    CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
    CCB_ACE|CCB_NOBLK|CCB_PPABS;    // ccb_flags 

    DestCCB->ccb_PIXC = gSpritePIXC;         // PIXC control 
    DestCCB->ccb_PRE0 = 0x00000016;     // Preamble (Uncoded 16) 
    DestCCB->ccb_PRE1 = 0x9E001800+(Run-1);     // Second preamble 
    DestCCB->ccb_SourcePtr = (CelData *)CelLine190; // Save the source ptr 
    DestCCB->ccb_XPos = (x1+ScreenXOffset)<<16;     // Set the x and y coord for start 
    DestCCB->ccb_YPos = gSpriteY+(Clip<<16);
    DestCCB->ccb_HDX = 0<<20;       // OK 
    DestCCB->ccb_HDY = 1<<20;
    DestCCB->ccb_VDX = 1<<15;       // Need 15 to fix the LFORM bug 
    DestCCB->ccb_VDY = 0<<16;
    ++DestCCB;          // Next CCB 

    CurrentCCB = DestCCB;   // Save the CCB pointer 
}
#endif

//----------------------------------------------------------------------------------------------------------------------
// Draws a clipped sprite to the screen
//----------------------------------------------------------------------------------------------------------------------
static void drawSpriteClip(const uint32_t x1, const uint32_t x2, const vissprite_t& visSprite) noexcept {
    gSpriteYScale = visSprite.yscale << 4;                     // Get scale Y factor
    gStartLinePtr = (std::byte*) visSprite.pSprite->pTexture;  // Get pointer to first line of data
    gSpriteWidth = visSprite.pSprite->width;
    /*
    gSpritePIXC = (visSprite.colormap & 0x8000) ? 0x9C81 : gLightTable[(visSprite.colormap & 0xFF) >> LIGHTSCALESHIFT];
    */
    uint32_t y = visSprite.y1;
    gSpriteY = (y + gScreenYOffset) << 16;    // Unmolested Y coord
    uint32_t y2 = visSprite.y2;
    uint32_t MaxRun = y2 - y;
    
    if ((int) y < 0) {
        y = 0;
    }
    
    if ((int) y2 >= (int) gScreenHeight) {
        y2 = gScreenHeight;
    }
    
    Fixed XFrac = 0;
    Fixed XStep = 0xFFFFFFFFUL / (uint32_t) visSprite.xscale;   // Get the recipocal for the X scale
    
    if (visSprite.colormap & 0x4000) {
        XStep = -XStep;     // Step in the opposite direction
        XFrac = (gSpriteWidth << FRACBITS) - 1;
    }
    
    if (visSprite.x1 != x1) {   // How far should I skip?
        XFrac += XStep * (x1 - visSprite.x1);
    }
    
    uint32_t x = x1;
    
    do {
        uint32_t top = gSprOpening[x];  // Get the opening to the screen
        
        if (top == gScreenHeight) {  // Not clipped?
            oneSpriteLine(x, XFrac, 0, gScreenHeight, visSprite);
        } else {
            uint32_t bottom = top & 0xff;
            top >>= 8;
            
            if (top < bottom) { // Valid run?
                if (y >= top && y2 < bottom) {
                    oneSpriteLine(x, XFrac, 0, gScreenHeight, visSprite);
                } else {
                    int Clip = top - visSprite.y1;      // Number of pixels to clip
                    int Run = bottom - top;             // Allowable run
                    
                    if (Clip < 0) {     // Overrun?
                        Run += Clip;    // Remove from run
                        Clip = 0;
                    }
                    
                    if (Run > 0) {              // Still visible?
                        if (Run > MaxRun) {     // Too big?
                            Run = MaxRun;       // Force largest...
                        }
                        
                        oneSpriteLine(x, XFrac, top, bottom, visSprite);
                    }
                }
            }
        }
        XFrac += XStep;
    } while (++x <= x2);

    #if 0
    Word y,MaxRun;
    Word y2;
    Word top,bottom;
    patch_t *patch;
    Fixed XStep,XFrac;
    
    patch = (patch_t *)loadResourceData(vis->PatchLump);   // Get shape data 
    patch =(patch_t *) &((Byte *)patch)[vis->PatchOffset];  // Get true pointer 
    gSpriteYScale = vis->yscale<<4;      // Get scale Y factor 
    gSpritePLUT = &((Byte *)patch)[64];  // Get pointer to PLUT 
    gSpritePRE0 = ((Word *)patch)[14]&~(0xFF<<6);    // Only 1 row allowed! 
    gSpritePRE1 = ((Word *)patch)[15];       // Get the proper height 
    y = ((Word *)patch)[3];     // Get offset to the sprite shape data 
    gStartLinePtr = &((Byte *)patch)[y+16];  // Get pointer to first line of data 
    gSpriteWidth = getCCBHeight(&((Word *)patch)[1]);
    gSpritePIXC = (vis->colormap&0x8000) ? 0x9C81 : gLightTable[(vis->colormap&0xFF)>>LIGHTSCALESHIFT];
    y = vis->y1;
    gSpriteY = (y+ScreenYOffset)<<16;    // Unmolested Y coord 
    y2 = vis->y2;
    MaxRun = y2-y;
    
    if ((int)y<0) {
        y = 0;
    }
    if ((int)y2>=(int)ScreenHeight) {
        y2 = ScreenHeight;
    }
    XFrac = 0;
    XStep = 0xFFFFFFFFUL/(LongWord)vis->xscale; // Get the recipocal for the X scale 
    if (vis->colormap&0x4000) {
        XStep = -XStep;     // Step in the opposite direction 
        XFrac = (gSpriteWidth<<FRACBITS)-1;
    }
    if (vis->x1!=x1) {      // How far should I skip? 
        XFrac += XStep*(x1-vis->x1);
    }
    do {
        top = spropening[x1];       // Get the opening to the screen 
        if (top==ScreenHeight) {        // Not clipped? 
            OneSpriteLine(x1,CalcLine(XFrac));
        } else {
            bottom = top&0xff;
            top >>=8;
            if (top<bottom) {       // Valid run? 
                if (y>=top && y2<bottom) {
                    OneSpriteLine(x1,CalcLine(XFrac));
                } else {
                    int Run;
                    int Clip;
                    
                    Clip = top-vis->y1;     // Number of pixels to clip 
                    Run = bottom-top;       // Allowable run 
                    if (Clip<0) {       // Overrun? 
                        Run += Clip;    // Remove from run 
                        Clip = 0;
                    }
                    if (Run>0) {        // Still visible? 
                        if (Run>MaxRun) {       // Too big? 
                            Run = MaxRun;       // Force largest... 
                        }
                        OneSpriteClipLine(x1,CalcLine(XFrac),Clip,Run);
                    }
                }
            }
        }
        XFrac+=XStep;
    } while (++x1<=x2);
    #endif
}


/**********************************

    Using a point in space, determine if it is BEHIND a wall.
    Use a cross product to determine facing.
    
**********************************/

static uint32_t SegBehindPoint(viswall_t *ds,Fixed dx,Fixed dy)
{
    Fixed x1,y1;
    Fixed sdx,sdy;
    const seg_t *SegPtr = ds->SegPtr;
    
    x1 = SegPtr->v1.x;
    y1 = SegPtr->v1.y;
    
    sdx = SegPtr->v2.x-x1;
    sdy = SegPtr->v2.y-y1;
    
    dx -= x1;
    dy -= y1;
    
    sdx>>=FRACBITS;
    sdy>>=FRACBITS;
    dx>>=FRACBITS;
    dy>>=FRACBITS;
    
    dx*=sdy;
    sdx*=dy;
    if (sdx<dx) {
        return true;
    } 
    return false;
}

//--------------------------------------------------------------------------------------------------
// See if a sprite needs clipping and if so, then draw it clipped
//--------------------------------------------------------------------------------------------------
void drawVisSprite(const vissprite_t& visSprite) noexcept {
    viswall_t *ds;
    int x, r1, r2;
    int silhouette;
    int x1, x2;
    uint8_t* topsil,*bottomsil;
    uint32_t opening;
    int top, bottom;
    uint32_t scalefrac;
    uint32_t Clipped;

    x1 = visSprite.x1;        // Get the sprite's screen posts
    x2 = visSprite.x2;
    if (x1<0) {                 // These could be offscreen
        x1 = 0;
    }
    if (x2>=(int)gScreenWidth) {
        x2 = gScreenWidth-1;
    }
    scalefrac = visSprite.yscale;     // Get the Z scale    
    Clipped = false;                    // Assume I don't clip

    // scan drawsegs from end to start for obscuring segs
    // the first drawseg that has a greater scale is the clip seg
    ds = gpEndVisWall;
    do {
        --ds;           // Point to the next wall command
        
        // determine if the drawseg obscures the sprite
        if (ds->LeftX > x2 || ds->RightX < x1 ||
            ds->LargeScale <= scalefrac ||
            !(ds->WallActions&(AC_TOPSIL|AC_BOTTOMSIL|AC_SOLIDSIL)) ) {
            continue;           // doesn't cover sprite
        }

        if (ds->SmallScale<=scalefrac) {    // In range of the wall?
            if (SegBehindPoint(ds,visSprite.thing->x, visSprite.thing->y)) {
                continue;           // Wall seg is behind sprite
            }
        }
        if (!Clipped) {     // Never initialized?
            Clipped = true;
            x = x1;
            opening = gScreenHeight;
            do {
                gSprOpening[x] = opening;       // Init the clip table
            } while (++x<=x2);
        }
        r1 = ds->LeftX < x1 ? x1 : ds->LeftX;       // Get the clip bounds
        r2 = ds->RightX > x2 ? x2 : ds->RightX;

        // clip this piece of the sprite
        silhouette = ds->WallActions & (AC_TOPSIL|AC_BOTTOMSIL|AC_SOLIDSIL);
        x=r1;
        if (silhouette == AC_SOLIDSIL) {
            opening = gScreenHeight<<8;
            do {
                gSprOpening[x] = opening;       // Clip these to blanks
            } while (++x<=r2);
            continue;
        }
        
        topsil = ds->TopSil;
        bottomsil = ds->BottomSil;

        if (silhouette == AC_BOTTOMSIL) {   // bottom sil only
            do {
                opening = gSprOpening[x];
                if ( (opening&0xff) == gScreenHeight) {
                    gSprOpening[x] = (opening&0xff00) + bottomsil[x];
                }
            } while (++x<=r2);
        } else if (silhouette == AC_TOPSIL) {   // top sil only
            do {
                opening = gSprOpening[x];
                if ( !(opening&0xff00)) {
                    gSprOpening[x] = (topsil[x]<<8) + (opening&0xff);
                }
            } while (++x<=r2);
        } else if (silhouette == (AC_TOPSIL|AC_BOTTOMSIL) ) {   // both
            do {
                top = gSprOpening[x];
                bottom = top&0xff;
                top >>= 8;
                if (bottom == gScreenHeight) {
                    bottom = bottomsil[x];
                }
                if (!top) {
                    top = topsil[x];
                }
                gSprOpening[x] = (top<<8)+bottom;
            } while (++x<=r2);
        }
    } while (ds!=gVisWalls);
    
    // Now that I have created the clip regions, let's see if I need to do this    
    if (!Clipped) {                     // Do I have to clip at all?
        drawSpriteNoClip(visSprite);    // Draw it using no clipping at all */
        return;                         // Exit
    }
    
    // Check the Y bounds to see if the clip rect even touches the sprite    
    r1 = visSprite.y1;
    r2 = visSprite.y2;
    if (r1<0) {
        r1 = 0;     // Clip to screen coords
    }
    if (r2>=(int)gScreenHeight) {
        r2 = gScreenHeight;
    }
    x = x1;
    do {
        top = gSprOpening[x];
        if (top!=gScreenHeight) {        // Clipped?
            bottom = top&0xff;
            top >>=8;
            if (r1<top || r2>=bottom) { // Needs manual clipping!
                if (x!=x1) {        // Was any part visible?
                    drawSpriteClip(x1,x2, visSprite);  // Draw it and exit
                    return;
                }
                do {
                    top = gSprOpening[x];
                    if (top!=(gScreenHeight<<8)) {
                        bottom = top&0xff;
                        top >>=8;
                        if (r1<bottom && r2>=top) { // Is it even visible?
                            drawSpriteClip(x1,x2, visSprite);      // Draw it
                            return;
                        }
                    }
                } while (++x<=x2);
                return;     // It's not visible at all!!
            }
        }
    } while (++x<=x2);
    drawSpriteNoClip(visSprite);      // It still didn't need clipping!!
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
