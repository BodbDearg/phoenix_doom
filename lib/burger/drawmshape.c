#include "burger.h"

// DC: 3DO specific headers - remove
#if 0
    #include <Graphics.h>
#endif

/**********************************

    Draw a masked shape on the screen

**********************************/

void DrawMShape(Word x,Word y,void *ShapePtr)
{
    // DC: FIXME: reimplement/replace
    #if 0
        ((CCB*)ShapePtr)->ccb_XPos = x<<16;     /* Set the X coord */
        ((CCB*)ShapePtr)->ccb_YPos = y<<16;     /* Set the Y coord */
        ((CCB*)ShapePtr)->ccb_Flags &= ~CCB_BGND;   /* Enable masking */
        DrawCels(VideoItem,(CCB*)ShapePtr);     /* Draw the shape */
    #endif
}
