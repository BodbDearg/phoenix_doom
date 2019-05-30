#include "burger.h"

// DC: 3DO specific headers - remove
#if 0
    #include <event.h>
#endif

/**********************************

    Read the bits from the mouse

**********************************/

void ReadMouseAbs(Word *x,Word *y)
{
    // DC: FIXME: reimplement/replace
    #if 0
        MouseEventData ControlRec;

        GetMouse(1,FALSE,&ControlRec);      /* Read mouse */
        *x = (Word)ControlRec.med_HorizPosition;
        *y = (Word)ControlRec.med_VertPosition;
    #else
        *x = 0;
        *y = 0;
    #endif
}
