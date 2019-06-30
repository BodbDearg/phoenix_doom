#include "Burger.h"

#include "Endian.h"
#include "Resources.h"
#include <cstring>
#include <ctime>
#include <SDL.h>

// DC: 3DO specific headers - remove
#if 0
    #include <event.h>
    #include <filefunctions.h>
    #include <filesystem.h>
    #include <Graphics.h>
    #include <io.h>
    #include <kernel.h>
    #include <operror.h>
    #include <task.h>
#endif

/* Width of the screen in pixels */
uint32_t FramebufferWidth = 320;
uint32_t FramebufferHeight = 200;

/**********************************

    Pointers used by the draw routines to describe
    the video display.

**********************************/

uint8_t *VideoPointer;     /* Pointer to draw buffer */
Item VideoItem;         /* 3DO specific video Item number for hardware */
Item VideoScreen;       /* 3DO specific screen Item number for hardware */

/**********************************

    Time mark for ticker

**********************************/

uint32_t LastTick;      /* Time last waited at */

/**********************************

    Read the current timer value

**********************************/

static volatile uint32_t TickValue;
static bool TimerInited;

//---------------------------------------------------------------------------------------------------------------------
// Draw a shape using a resource number
//---------------------------------------------------------------------------------------------------------------------
void DrawRezShape(uint32_t x, uint32_t y, uint32_t RezNum) noexcept {
    DrawShape(x, y, reinterpret_cast<const CelControlBlock*>(loadResourceData(RezNum)));
    releaseResource(RezNum);
}

/**********************************

    Return the pointer of a shape from a shape array

**********************************/

const struct CelControlBlock* GetShapeIndexPtr(const void* ShapeArrayPtr, uint32_t Index) noexcept
{
    const uint32_t* const pShapeArrayOffsets = (const uint32_t*) ShapeArrayPtr;
    const uint32_t shapeArrayOffset = byteSwappedU32(pShapeArrayOffsets[Index]);
    return (const CelControlBlock*) &((Byte*) ShapeArrayPtr)[shapeArrayOffset];
}


/**********************************

    Read the bits from the joystick

**********************************/

uint32_t LastJoyButtons[4];     /* Save the previous joypad bits */

uint32_t ReadJoyButtons(uint32_t PadNum) noexcept
{
    // DC: FIXME: TEMP
    uint32_t buttons = 0;
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

/******************************

    This is my IRQ task executed 60 times a second
    This is spawned as a background thread and modifies a
    global timer variable.

******************************/

// DC: FIXME: reimplement/replace
#if 0
static void Timer60Hz(void) noexcept
{
    Item devItem;       /* Item referance for a timer device */
    Item IOReqItem;     /* IO Request item */
    IOInfo ioInfo;      /* IO Info struct */
    struct timeval tv;  /* Timer time struct */

    devItem = OpenNamedDevice("timer",0);       /* Open the timer */
    IOReqItem = CreateIOReq(0,0,devItem,0);     /* Create a IO request */

    for (;;) {  /* Stay forever */
        tv.tv_sec = 0;
        tv.tv_usec = 16667;                     /* 60 times per second */
        memset(&ioInfo,0,sizeof(ioInfo));       /* Init the struct */
        ioInfo.ioi_Command = TIMERCMD_DELAY;    /* Sleep for some time */
        ioInfo.ioi_Unit = TIMER_UNIT_USEC;
        ioInfo.ioi_Send.iob_Buffer = &tv;       /* Pass the time struct */
        ioInfo.ioi_Send.iob_Len = sizeof(tv);
        DoIO(IOReqItem,&ioInfo);                /* Perform the task sleep */
        ++TickValue;                            /* Inc at 60 hz */
    }
}
#endif

uint32_t ReadTick() noexcept
{
    if (TimerInited) {      /* Was the timer started? */
        return TickValue;
    }
    
    TimerInited = true;     /* Mark as started */

    // DC: FIXME: reimplement/replace
    #if 0
        CreateThread("Timer60Hz",KernelBase->kb_CurrentTask->t.n_Priority+10,Timer60Hz,512);
    #endif
    
    return TickValue;       /* Return the tick value */
}

/****************************************

    Prints an unsigned long number.
    Also prints lead spaces or zeros if needed

****************************************/

static uint32_t TensTable[] = {
1,              /* Table to quickly div by 10 */
10,
100,
1000,
10000,
100000,
1000000,
10000000,
100000000,
1000000000
};

void LongWordToAscii(uint32_t Val, char* AsciiPtr) noexcept
{
    uint32_t Index;      /* Index to TensTable */
    uint32_t BigNum;    /* Temp for TensTable value */
    uint32_t Letter;        /* ASCII char */
    uint32_t Printing;      /* Flag for printing */

    Index = 10;      /* 10 digits to process */
    Printing = false;   /* Not printing yet */
    do {
        --Index;        /* Dec index */
        BigNum = TensTable[Index];  /* Get div value in local */
        Letter = '0';            /* Init ASCII value */
        while (Val>=BigNum) {    /* Can I divide? */
            Val-=BigNum;        /* Remove value */
            ++Letter;            /* Inc ASCII value */
        }
        if (Printing || Letter!='0' || !Index) {    /* Already printing? */
            Printing = true;        /* Force future printing */
            AsciiPtr[0] = Letter;       /* Also must print on last char */
            ++AsciiPtr;
        }
    } while (Index);        /* Any more left? */
    AsciiPtr[0] = 0;        /* Terminate the string */
}

/**********************************

    Save a file to NVRAM or disk

**********************************/

uint32_t SaveAFile(Byte *name, void *data, uint32_t dataSize) noexcept
{
    // DC: FIXME: reimplement/replace
    return -1;
}
