#include "burger.h"

// DC: 3DO specific headers - remove
#if 0
    #include <Graphics.h>
#endif

/**********************************

    Draw an unmasked shape on the screen

**********************************/

void DrawShape(Word x,Word y,void *ShapePtr)
{
    // DC: FIXME: reimplement/replace
    #if 0
        ((CCB*)ShapePtr)->ccb_XPos = x<<16;         /* Set the X coord */
        ((CCB*)ShapePtr)->ccb_YPos = y<<16;         /* Set the Y coord */
        ((CCB*)ShapePtr)->ccb_Flags |= CCB_BGND;    /* Disable masking */
        DrawCels(VideoItem,(CCB*)ShapePtr);         /* Draw the shape */
    #endif
}
