#include "burger.h"

// DC: 3DO specific header - remove
#if 0
    #include <Graphics.h>
#endif

/**********************************

    Erase a masked shape from the screen

**********************************/

void EraseMBShape(Word x,Word y,void *ShapePtr,void *BackPtr)
{
    // DC: FIXME: Reimplement/replace
    #if 0
        LongWord TempPIXC;

        TempPIXC = ((CCB*)ShapePtr)->ccb_PIXC;
        ((CCB*)ShapePtr)->ccb_PIXC = TempPIXC | 0x80008000; /* Cel engine #1 and #2 */
        SetReadAddress(VideoItem,(char *)BackPtr,320);      /* Set the read address to offscreen buffer */
        DrawMShape(x,y,ShapePtr);               /* Draw the masked shape */
        ((CCB*)ShapePtr)->ccb_PIXC = TempPIXC;  /* Restore the bits */
        ResetReadAddress(VideoItem);            /* Restore the read address */
    #endif
}
