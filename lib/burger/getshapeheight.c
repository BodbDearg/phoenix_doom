#include "burger.h"

// DC: 3DO specific header - remove
#if 0
    #include <Graphics.h>
#endif

/**********************************

    Return the height of a shape in pixels

**********************************/

Word GetShapeHeight(void *ShapePtr)
{
    // DC: FIXME: Reimplement/replace
    #if 0
        Word Result;
        Result = ((CCB*)ShapePtr)->ccb_PRE0>>6; /* Get the VCount bits */
        Result &= 0x3FF;            /* Mask off unused bits */
        return Result+1;            /* Return the TRUE result */
    #else
        return 0;
    #endif
}
