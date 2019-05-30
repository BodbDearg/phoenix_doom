#include "burger.h"

// DC: 3DO specific headers - remove
#if 0
    #include <event.h>
#endif

/**********************************

    Read the bits from the mouse

**********************************/

Word LastMouseButton;

Word ReadMouseButtons(void)
{
    // DC: FIXME: reimplement/replace
    #if 0
        MouseEventData ControlRec;
        GetMouse(1,FALSE,&ControlRec);                      /* Read mouse */
        LastMouseButton = (Word) ControlRec.med_ButtonBits; /* Return the data */
        return (Word) ControlRec.med_ButtonBits;            /* Return the data */
    #else
        return 0;
    #endif
}
