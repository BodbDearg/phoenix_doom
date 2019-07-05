#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Video.h"

BEGIN_NAMESPACE(Renderer)

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
