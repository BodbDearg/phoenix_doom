#include "Base/Mem.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Render.h"
#include "Sprites.h"
#include "Textures.h"

#define STRETCH(WIDTH,HEIGHT) (Fixed)((160.0/(float)WIDTH)*((float)HEIGHT/180.0)*2.2*65536)     

static uint32_t gScreenWidths[6] = { 280, 256, 224, 192, 160, 128};
static uint32_t gScreenHeights[6] = { 160, 144, 128, 112, 96, 80};
static Fixed gStretchs[6] = {
    STRETCH(280,160),
    STRETCH(256,144),
    STRETCH(224,128),
    STRETCH(192,112),
    STRETCH(160, 96),
    STRETCH(128, 80)
};

/**********************************

    Load in the "TextureInfo" array so that the game knows
    all about the wall and sky textures (Width,Height).
    Also initialize the texture translation table for wall animations.
    Called on powerup only.

**********************************/
void R_InitData() {
    texturesInit();
    spritesInit();
    
    // Create a recipocal mul table so that I can divide 0-8191 from 1.0.
    // This way I can fake a divide with a multiply.
    gIDivTable[0] = -1;
    
    {
        uint32_t i = 1;
    
        do {
            gIDivTable[i] = fixedDiv(512 << FRACBITS, i << FRACBITS);    // 512.0 / i
        } while (++i < (sizeof(gIDivTable) / sizeof(uint32_t)));
    }
    
    InitMathTables();
}

/**********************************

    Create all the math tables for the current screen size

**********************************/
void InitMathTables() {
    Fixed j;
    uint32_t i;
    
    gScreenWidth = gScreenWidths[gScreenSize];
    gScreenHeight = gScreenHeights[gScreenSize];
    gCenterX = (gScreenWidth/2);
    gCenterY = (gScreenHeight/2);
    gScreenXOffset = ((320-gScreenWidth)/2);
    gScreenYOffset = ((160-gScreenHeight)/2);
    gGunXScale = (gScreenWidth*0x100000)/320;     // Get the 3DO scale factor for the gun shape 
    gGunYScale = (gScreenHeight*0x10000)/160;     // And the y scale 

    gStretch = gStretchs[gScreenSize];
    gStretchWidth = gStretch*((int)gScreenWidth/2);

    // Create the viewangletox table     
    j = fixedDiv(gCenterX << FRACBITS, gFineTangent[FINEANGLES / 4 + FIELDOFVIEW / 2]);
    i = 0;
    do {
        Fixed t;
        if (gFineTangent[i]>FRACUNIT*2) {
            t = -1;
        } else if (gFineTangent[i]< -FRACUNIT*2) {
            t = gScreenWidth+1;
        } else {
            t = fixedMul(gFineTangent[i] , j);
            t = ((gCenterX<<FRACBITS)-t+FRACUNIT-1)>>FRACBITS;
            if (t<-1) {
                t = -1;
            } else if (t>(int)gScreenWidth+1) {
                t = gScreenWidth+1;
            }
        }
        gViewAngleToX[i/2] = t;
        i+=2;
    } while (i<FINEANGLES/2);

    // Using the viewangletox, create xtoviewangle table 
        
    i = 0;
    do {
        uint32_t x;
        x = 0;
        while (gViewAngleToX[x]>(int)i) {
            ++x;
        }
        gXToViewAngle[i] = (x<<(ANGLETOFINESHIFT+1))-ANG90;
    } while (++i<gScreenWidth+1);
    
    // Set the minimums and maximums for viewangletox 
    i = 0;
    do {
        if (gViewAngleToX[i]==-1) {
            gViewAngleToX[i] = 0;
        } else if (gViewAngleToX[i] == gScreenWidth+1) {
            gViewAngleToX[i] = gScreenWidth;
        }
    } while (++i<FINEANGLES/4);
    
    // Make the yslope table for floor and ceiling textures 
    
    i = 0;
    do {
        j = (((int)i-(int)gScreenHeight/2)*FRACUNIT)+FRACUNIT/2;
        j = fixedDiv(gStretchWidth, abs(j));
        j >>= 6;
        if (j>0xFFFF) {
            j = 0xFFFF;
        }
        gYSlope[i] = j;
    } while (++i<gScreenHeight);
    
    // Create the distance scale table for floor and ceiling textures 
    
    i = 0;
    do {
        j = abs(gFineCosine[gXToViewAngle[i]>>ANGLETOFINESHIFT]);
        gDistScale[i] = fixedDiv(FRACUNIT, j) >> 1;
    } while (++i<gScreenWidth);

    // Create the lighting tables 
    
    i = 0;
    do {
        Fixed Range;
        j = i/3;
        gLightMins[i] = j;   // Save the light minimum factors 
        Range = i-j;
        gLightSubs[i] = ((Fixed)gScreenWidth*Range)/(800-(Fixed)gScreenWidth);
        gLightCoefs[i] = (Range<<16)/(800-(Fixed)gScreenWidth);
        gPlaneLightCoef[i] = Range*(0x140000/(800-(Fixed)gScreenWidth));
    } while (++i<256);
}
