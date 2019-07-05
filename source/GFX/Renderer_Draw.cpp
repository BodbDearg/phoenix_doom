#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Sprites.h"
#include "Textures.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

// TODO: CLEANUP
#define LIGHTSCALESHIFT 3
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

/**********************************

    Ok boys amd girls, the follow functions are drawing primitives
    for Doom 3DO that take into account the fact that I draw all lines from
    back to front. This way I use painter's algorithm to do clipping since the
    3DO cel engine does not allow arbitrary clipping of any kind.
    
    On the first pass, I draw all the sky lines without any regard for clipping
    since no object can be behind the sky.
    
    Next I then sort the sprites from back to front.
    
    Then I draw the walls from back to front, mixing in all the sprites so
    that all shapes will clip properly using painter's algorithm.

**********************************/

#define MAX_WALL_LIGHT_VALUE 15
#define MAX_FLOOR_LIGHT_VALUE 15
#define MAX_SPRITE_LIGHT_VALUE 31

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

void DrawSkyLine()
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

//---------------------------------------------------------------------------------------------------------------------
// Returns a fixed point multipler for the given texture light value (which is in 4.3 format)
// This can be used to scale RGB values due to lighting.
//---------------------------------------------------------------------------------------------------------------------
static Fixed getLightMultiplier(const uint32_t lightValue, const uint32_t maxLightValue) {
    const Fixed maxLightValueFrac = intToFixed(maxLightValue);
    const Fixed textureLightFrac = lightValue << (FRACBITS - LIGHTSCALESHIFT);
    const Fixed lightMultiplier = fixedDiv(textureLightFrac, maxLightValueFrac);
    return (lightMultiplier > FRACUNIT) ? FRACUNIT : lightMultiplier;
}

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

void DrawWallColumn(
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

void DrawFloorColumn(
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

    Perform a "Doom" like screen wipe
    I require that VideoPointer is set to the current screen

**********************************/

#define WIPEWIDTH 320       // Width of the 3DO screen to wipe 
#define WIPEHEIGHT 200

// DC: TODO: unused currently
#if 0
void WipeDoom(LongWord *OldScreen,LongWord *NewScreen)
{
    LongWord Mark;  // Last time mark 
    Word TimeCount; // Elapsed time since last mark 
    Word i,x;
    Word Quit;      // Finish flag 
    int delta;      // YDelta (Must be INT!) 
    LongWord *Screenad;     // I use short pointers so I can 
    LongWord *SourcePtr;        // move in pixel pairs... 
    int YDeltaTable[WIPEWIDTH/2];   // Array of deltas for the jagged look 

// First thing I do is to create a ydelta table to 
// allow the jagged look to the screen wipe 

    delta = -GetRandom(15); // Get the initial position 
    YDeltaTable[0] = delta; // Save it 
    x = 1;
    do {
        delta += (GetRandom(2)-1);  // Add -1,0 or 1 
        if (delta>0) {      // Too high? 
            delta = 0;
        }
        if (delta == -16) { // Too low? 
            delta = -15;
        }
        YDeltaTable[x] = delta; // Save the delta in table 
    } while (++x<(WIPEWIDTH/2));    // Quit? 

// Now perform the wipe using ReadTick to generate a time base 
// Do NOT go faster than 30 frames per second 

    Mark = ReadTick()-2;    // Get the initial time mark 
    do {
        do {
            TimeCount = ReadTick()-Mark;    // Get the time mark 
        } while (TimeCount<(TICKSPERSEC/30));           // Enough time yet? 
        Mark+=TimeCount;        // Adjust the base mark 
        TimeCount/=(TICKSPERSEC/30);    // Math is for 30 frames per second 

// Adjust the YDeltaTable "TimeCount" times to adjust for slow machines 

        Quit = true;        // Assume I already am finished 
        do {
            x = 0;      // Start at the left 
            do {
                delta = YDeltaTable[x];     // Get the delta 
                if (delta<WIPEHEIGHT) { // Line finished? 
                    Quit = false;       // I changed one! 
                    if (delta < 0) {
                        ++delta;        // Slight delay 
                    } else if (delta < 16) {
                        delta = delta<<1;   // Double it 
                        ++delta;
                    } else {
                        delta+=8;       // Constant speed 
                        if (delta>WIPEHEIGHT) {
                            delta=WIPEHEIGHT;
                        }
                    }
                    YDeltaTable[x] = delta; // Save new delta 
                }
            } while (++x<(WIPEWIDTH/2));
        } while (--TimeCount);      // All tics accounted for? 

// Draw a frame of the wipe 

        x = 0;          // Init the x coord 
        do {
            Screenad = (LongWord *)&VideoPointer[x*8];  // Dest pointer 
            i = YDeltaTable[x];     // Get offset 
            if ((int)i<0) { // Less than zero? 
                i = 0;      // Make it zero 
            }
            i>>=1;      // Force even for 3DO weirdness 
            if (i) {
                TimeCount = i;
                SourcePtr = &NewScreen[x*2];    // Fetch from source 
                do {
                    Screenad[0] = SourcePtr[0]; // Copy 2 longwords 
                    Screenad[1] = SourcePtr[1];
                    Screenad+=WIPEWIDTH;
                    SourcePtr+=WIPEWIDTH;
                } while (--TimeCount);
            }
            if (i<(WIPEHEIGHT/2)) {     // Any of the old image to draw? 
                i = (WIPEHEIGHT/2)-i;
                SourcePtr = &OldScreen[x*2];
                do {
                    Screenad[0] = SourcePtr[0]; // Copy 2 longwords 
                    Screenad[1] = SourcePtr[1];
                    Screenad+=WIPEWIDTH;
                    SourcePtr+=WIPEWIDTH;
                } while (--i);
            }
        } while (++x<(WIPEWIDTH/2));
    } while (!Quit);        // Wipe finished? 
}
#endif

static std::byte* gSpritePLUT;
static uint32_t gSpriteY;
static uint32_t gSpriteYScale;
static uint32_t gSpritePIXC;
static uint32_t gSpritePRE0;
static uint32_t gSpritePRE1;
static std::byte* gStartLinePtr;
static uint32_t gSpriteWidth;

//-------------------------------------------------------------------------------------------------
// Get the pointer to the first pixel in a particular column of a sprite.
// The input x coordinate is in 16.16 fixed point format and the output is clamped to sprite bounds.
//-------------------------------------------------------------------------------------------------
static const uint16_t* getSpriteColumn(const vissprite_t& visSprite, const Fixed xFrac) {
    const int32_t x = fixedToInt(xFrac);
    const SpriteFrameAngle* const pSprite = visSprite.pSprite;
    const int32_t xClamped = (x < 0) ? 0 : ((x >= pSprite->width) ? pSprite->width - 1 : x);
    return &pSprite->pTexture[pSprite->height * xClamped];
}

//-------------------------------------------------------------------------------------------------
// Utility that determines how much to step (in texels) per pixel to render the entire of the given
// sprite dimension in the given render area dimension (both in pixels).
//-------------------------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------------------------
// This routine will draw a scaled sprite during the game. It is called when there is no 
// onscreen clipping needed or if the only clipping is to the screen bounds.
//-------------------------------------------------------------------------------------------------
void drawSpriteNoClip(const vissprite_t& visSprite) noexcept {
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

//-------------------------------------------------------------------------------------------------
// Draws a column of a sprite that is clipped to the given top and bottom bounds
//-------------------------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------------------------
// Draws a clipped sprite to the screen
//-------------------------------------------------------------------------------------------------
void drawSpriteClip(const uint32_t x1, const uint32_t x2, const vissprite_t& visSprite) noexcept {
    gSpriteYScale = visSprite.yscale << 4;                     // Get scale Y factor
    gStartLinePtr = (std::byte*) visSprite.pSprite->pTexture;  // Get pointer to first line of data
    gSpriteWidth = visSprite.pSprite->width;
    gSpritePIXC = (visSprite.colormap & 0x8000) ? 0x9C81 : gLightTable[(visSprite.colormap & 0xFF) >> LIGHTSCALESHIFT];
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

    Draw a sprite in the center of the screen.
    This is used for the finale.
    (Speed is NOT an issue here...)

**********************************/

void DrawSpriteCenter(uint32_t SpriteNum)
{
    // DC: FIXME: implement/replace
    #if 0
        Word x,y;
        patch_t *patch;
        LongWord Offset;

        patch = (patch_t *)LoadAResource(SpriteNum>>FF_SPRITESHIFT);    // Get the sprite group 
        Offset = ((LongWord *)patch)[SpriteNum & FF_FRAMEMASK];
        if (Offset&PT_NOROTATE) {       // Do I rotate? 
            patch = (patch_t *) &((Byte *)patch)[Offset & 0x3FFFFFFF];      // Get pointer to rotation list 
            Offset = ((LongWord *)patch)[0];        // Use the rotated offset 
        }
        patch = (patch_t *)&((Byte *)patch)[Offset & 0x3FFFFFFF];   // Get pointer to patch 
    
        x = patch->leftoffset;      // Get the x and y offsets 
        y = patch->topoffset;
        x = 80-x;           // Center on the screen 
        y = 90-y;
        ((LongWord *)patch)[7] = 0;     // Compensate for sideways scaling 
        ((LongWord *)patch)[10] = 0;
        if (Offset&PT_FLIP) {
            ((LongWord *)patch)[8] = -0x2<<20;  // Reverse horizontal 
            x+=GetShapeHeight(&patch->Data);    // Adjust the x coord 
        } else {
            ((LongWord *)patch)[8] = 0x2<<20;   // Normal horizontal 
        }
        ((LongWord *)patch)[9] = 0x2<<16;       // Double vertical 
        DrawMShape(x*2,y*2,&patch->Data);       // Scale the x and y and draw 
        ReleaseAResource(SpriteNum>>FF_SPRITESHIFT);    // Let go of the resource 
    #endif
}

/**********************************

    Draw a screen color overlay if needed
    
**********************************/

void DrawColors()
{
    return;

    // TODO: TEST INVULNERABILITY EFFECT
    for (int y = 0; y <= gScreenHeight; ++y) {
        uint32_t* pPixel = &Video::gFrameBuffer[gScreenXOffset + (gScreenYOffset + y) * Video::SCREEN_WIDTH];
        uint32_t* const pEndPixel = pPixel + gScreenWidth;

        while (pPixel < pEndPixel) {
            const uint32_t color = *pPixel;
            *pPixel = (~color) | 0x000000FF;
            ++pPixel;
        }
    }

    // DC: FIXME: implement/replace
    #if 0
        MyCCB* DestCCB;         // Pointer to new CCB entry 
        player_t *player;
        Word ccb,color;
        Word red,green,blue;
    
        player = &players;
        if (player->powers[pw_invulnerability] > 240        // Full strength 
            || (player->powers[pw_invulnerability]&16) ) {  // Flashing... 
            color = 0x7FFF<<16;
            ccb = CCB_LDSIZE|CCB_LDPRS|CCB_PXOR|
            CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
            CCB_ACE|CCB_BGND|CCB_NOBLK;
            goto DrawIt;
        }
    
        red = player->damagecount;      // Get damage inflicted 
        green = player->bonuscount>>2;
        red += green;
        blue = 0;
    
        if (player->powers[pw_ironfeet] > 240       // Radiation suit? 
            || (player->powers[pw_ironfeet]&16) ) { // Flashing... 
            green = 10;         // Add some green 
        }

        if (player->powers[pw_strength]         // Berserker pack? 
            && (player->powers[pw_strength]< 255) ) {
            color = 255-player->powers[pw_strength];
            color >>= 4;
            red+=color;     // Feeling good! 
        }

        if (red>=32) {
            red = 31;
        }
        if (green>=32) {
            green =31;
        }
        if (blue>=32) {
            blue = 31;
        }

        color = (red<<10)|(green<<5)|blue;
    
        if (!color) {
            return;
        }
        color <<=16;
        ccb = CCB_LDSIZE|CCB_LDPRS|
            CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
            CCB_ACE|CCB_BGND|CCB_NOBLK;

    DrawIt:
        DestCCB = CurrentCCB;       // Copy pointer to local 
        if (DestCCB>=&gCCBArray[CCBTotal]) {     // Am I full already? 
            FlushCCBs();                // Draw all the CCBs/Lines 
            DestCCB=gCCBArray;
        }
    
        DestCCB->ccb_Flags =ccb;    // ccb_flags 
        DestCCB->ccb_PIXC = 0x1F80;     // PIXC control 
        DestCCB->ccb_PRE0 = 0x40000016;     // Preamble 
        DestCCB->ccb_PRE1 = 0x03FF1000;     // Second preamble 
        DestCCB->ccb_SourcePtr = (CelData *)0;  // Save the source ptr 
        DestCCB->ccb_PLUTPtr = (void *)color;       // Set the color pixel 
        DestCCB->ccb_XPos = ScreenXOffset<<16;      // Set the x and y coord for start 
        DestCCB->ccb_YPos = ScreenYOffset<<16;
        DestCCB->ccb_HDX = ScreenWidth<<20;     // OK 
        DestCCB->ccb_HDY = 0<<20;
        DestCCB->ccb_VDX = 0<<16;
        DestCCB->ccb_VDY = ScreenHeight<<16;
        ++DestCCB;          // Next CCB 
        CurrentCCB = DestCCB;   // Save the CCB pointer 
    #endif
}

END_NAMESPACE(Renderer)
