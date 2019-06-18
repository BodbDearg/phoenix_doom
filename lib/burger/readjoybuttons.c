#include "burger.h"

#include <SDL.h>

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
    // DC: FIXME: TEMP
    Word buttons = 0;
    const Uint8 *state = SDL_GetKeyboardState(NULL);

    if (state[SDL_SCANCODE_UP]) {
        buttons |= PadUp;
    }

    if (state[SDL_SCANCODE_DOWN]) {
        buttons |= PadDown;
    }

    if (state[SDL_SCANCODE_LEFT]) {
        buttons |= PadLeft;
    }

    if (state[SDL_SCANCODE_RIGHT]) {
        buttons |= PadRight;
    }

    if (state[SDL_SCANCODE_A]) {
        buttons |= PadA;
    }

    if (state[SDL_SCANCODE_S]) {
        buttons |= PadB;
    }

    if (state[SDL_SCANCODE_D]) {
        buttons |= PadC;
    }

    if (state[SDL_SCANCODE_Z]) {
        buttons |= PadX;
    }

    if (state[SDL_SCANCODE_X]) {
        buttons |= PadStart;
    }

    if (state[SDL_SCANCODE_Q]) {
        buttons |= PadLeftShift;
    }

    if (state[SDL_SCANCODE_E]) {
        buttons |= PadRightShift;
    }

    return buttons;
    
    /*
#define PadDown         0x80000000
#define PadUp           0x40000000
#define PadRight        0x20000000
#define PadLeft         0x10000000
#define PadA            0x08000000
#define PadB            0x04000000
#define PadC            0x02000000
#define PadD            0x00000000
#define PadStart        0x01000000
#define PadX            0x00800000
#define PadRightShift   0x00400000
#define PadLeftShift    0x00200000
#define PadXLeft        0x00100000
#define PadXRight       0x00080000

#define RatLeft         0x80000000
#define RatMiddle       0x40000000
#define RatRight        0x20000000    
    */

    // DC: FIXME: reimplement or replace
    #if 0
        ControlPadEventData ControlRec;

        GetControlPad(PadNum+1,FALSE,&ControlRec);      /* Read joypad */
        if (PadNum<4) {
            LastJoyButtons[PadNum] = (Word)ControlRec.cped_ButtonBits;
        }
        return (Word)ControlRec.cped_ButtonBits;        /* Return the data */
    #endif
}
