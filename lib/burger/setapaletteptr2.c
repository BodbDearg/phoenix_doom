#include "burger.h"
#include <string.h>

// DC: 3DO specific headers - remove
#if 0
    #include <Graphics.h>
#endif

/**********************************

    Sets the colors of the palette

**********************************/

void SetAPalettePtr2(Word Start,Word Count,void *PalPtr)
{
    // DC: FIXME: reimplement/replace
    #if 0
        VDLEntry Temp;
        LongWord *OldPalPtr;

        if (!Count) {       /* Any colors to transfer? */
            return;
        }
        if (!Start && Count==32) {      /* Full palette update? */
            memcpy((char *)CurrentPalette,(char *)PalPtr,32*4);
            SetScreenColors(VideoScreen,(VDLEntry *)PalPtr,Count);
            return;             /* Exit now */
        }
        OldPalPtr = (LongWord*) &CurrentPalette[0]; /* Make a running pointer */
        do {
            Temp = ((LongWord *)PalPtr)[0];     /* Get the color entry */
            Temp = (Temp&0xFFFFFF)|(Start<<24); /* Replace the high byte */
            OldPalPtr[Start] = Temp;            /* Save in current palette */
            SetScreenColor(VideoScreen,Temp);   /* Set the color */
            ++Start;                            /* Next index */
            PalPtr = ((Byte*)PalPtr)+4;         /* Next PalPtr index */
        } while (--Count);
    #endif
}
