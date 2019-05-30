#include "burger.h"

// DC: 3DO specific - remove
#if 0
    #include <Graphics.h>
#endif

/**********************************

    Return the width of a shape in pixels

**********************************/

Word GetShapeWidth(void *ShapePtr)
{
    // DC: FIXME: replace or reimplement
    #if 0
        Word Result;
        Result = ((CCB*)ShapePtr)->ccb_PRE1&0x7FF;  /* Get the HCount bits */
        return Result+1;            /* Return the TRUE result */
    #else
        return 0;
    #endif
}
