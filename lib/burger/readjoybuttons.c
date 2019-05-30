#include "burger.h"

// DC: 3DO specific headers - remove
#if 0
    #include <event.h>
#endif

/**********************************

    Read the bits from the joystick

**********************************/

Word LastJoyButtons[4];     /* Save the previous joypad bits */

Word ReadJoyButtons(Word PadNum)
{
    // DC: FIXME: reimplement or replace
    #if 0
        ControlPadEventData ControlRec;

        GetControlPad(PadNum+1,FALSE,&ControlRec);      /* Read joypad */
        if (PadNum<4) {
            LastJoyButtons[PadNum] = (Word)ControlRec.cped_ButtonBits;
        }
        return (Word)ControlRec.cped_ButtonBits;        /* Return the data */
    #else
        return 0;
    #endif
}
