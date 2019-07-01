#include "Data.h"
#include "MathUtils.h"
#include "Mem.h"
#include "Render.h"
#include "Sprites.h"
#include "Tables.h"
#include "Textures.h"

#define STRETCH(WIDTH,HEIGHT) (Fixed)((160.0/(float)WIDTH)*((float)HEIGHT/180.0)*2.2*65536)     

static uint32_t ScreenWidths[6] = { 280, 256, 224, 192, 160, 128};
static uint32_t ScreenHeights[6] = { 160, 144, 128, 112, 96, 80};
static Fixed Stretchs[6] = {
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
    IDivTable[0] = -1;
    
    {
        uint32_t i = 1;
    
        do {
            IDivTable[i] = sfixedDiv16_16(512 << FRACBITS, i << FRACBITS);    // 512.0 / i
        } while (++i < (sizeof(IDivTable) / sizeof(uint32_t)));
    }
    
    InitMathTables();
}

/**********************************

    Create all the math tables for the current screen size

**********************************/
void InitMathTables() {
    Fixed j;
    uint32_t i;
    
    ScreenWidth = ScreenWidths[ScreenSize];
    ScreenHeight = ScreenHeights[ScreenSize];
    CenterX = (ScreenWidth/2);
    CenterY = (ScreenHeight/2);
    ScreenXOffset = ((320-ScreenWidth)/2);
    ScreenYOffset = ((160-ScreenHeight)/2);
    GunXScale = (ScreenWidth*0x100000)/320;     /* Get the 3DO scale factor for the gun shape */
    GunYScale = (ScreenHeight*0x10000)/160;     /* And the y scale */

    Stretch = Stretchs[ScreenSize];
    StretchWidth = Stretch*((int)ScreenWidth/2);

    /* Create the viewangletox table */    
    j = sfixedDiv16_16(CenterX << FRACBITS, finetangent[FINEANGLES / 4 + FIELDOFVIEW / 2]);
    i = 0;
    do {
        Fixed t;
        if (finetangent[i]>FRACUNIT*2) {
            t = -1;
        } else if (finetangent[i]< -FRACUNIT*2) {
            t = ScreenWidth+1;
        } else {
            t = sfixedMul16_16(finetangent[i] , j);
            t = ((CenterX<<FRACBITS)-t+FRACUNIT-1)>>FRACBITS;
            if (t<-1) {
                t = -1;
            } else if (t>(int)ScreenWidth+1) {
                t = ScreenWidth+1;
            }
        }   
        viewangletox[i/2] = t;
        i+=2;
    } while (i<FINEANGLES/2);

    /* Using the viewangletox, create xtoviewangle table */
        
    i = 0;
    do {
        uint32_t x;
        x = 0;
        while (viewangletox[x]>(int)i) {
            ++x;
        }
        xtoviewangle[i] = (x<<(ANGLETOFINESHIFT+1))-ANG90;
    } while (++i<ScreenWidth+1);
    
    /* Set the minimums and maximums for viewangletox */
    i = 0;
    do {
        if (viewangletox[i]==-1) {
            viewangletox[i] = 0;
        } else if (viewangletox[i] == ScreenWidth+1) {
            viewangletox[i] = ScreenWidth;
        }
    } while (++i<FINEANGLES/4);
    
    /* Make the yslope table for floor and ceiling textures */
    
    i = 0;
    do {
        j = (((int)i-(int)ScreenHeight/2)*FRACUNIT)+FRACUNIT/2;
        j = sfixedDiv16_16(StretchWidth, abs(j));
        j >>= 6;
        if (j>0xFFFF) {
            j = 0xFFFF;
        }
        yslope[i] = j;
    } while (++i<ScreenHeight);
    
    /* Create the distance scale table for floor and ceiling textures */
    
    i = 0;
    do {
        j = abs(finecosine[xtoviewangle[i]>>ANGLETOFINESHIFT]);
        distscale[i] = sfixedDiv16_16(FRACUNIT, j) >> 1;
    } while (++i<ScreenWidth);

    /* Create the lighting tables */
    
    i = 0;
    do {
        Fixed Range;
        j = i/3;
        lightmins[i] = j;   /* Save the light minimum factors */
        Range = i-j;
        lightsubs[i] = ((Fixed)ScreenWidth*Range)/(800-(Fixed)ScreenWidth);
        lightcoefs[i] = (Range<<16)/(800-(Fixed)ScreenWidth);
        planelightcoef[i] = Range*(0x140000/(800-(Fixed)ScreenWidth));
    } while (++i<256);
}
