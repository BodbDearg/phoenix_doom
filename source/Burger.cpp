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
Word FramebufferWidth = 320;
Word FramebufferHeight = 200;

/**********************************

    Variable for sound and other global system states

**********************************/

Word SystemState = 3;    /* Enable Sound/Music as default */

/**********************************

    Pointers used by the draw routines to describe
    the video display.

**********************************/

Byte *VideoPointer;     /* Pointer to draw buffer */
Item VideoItem;         /* 3DO specific video Item number for hardware */
Item VideoScreen;       /* 3DO specific screen Item number for hardware */

/**********************************

    Time mark for ticker

**********************************/

LongWord LastTick;      /* Time last waited at */

/**********************************

    Read the current timer value

**********************************/

static volatile LongWord TickValue;
static bool TimerInited;

/**********************************

    Draw a solid colored rect on the screen
    
**********************************/

// DC: FIXME: reimplement/replace
#if 0
    static LongWord MyCelData;  /* Color to draw with CCB */

    static CCB MyCCB2 = {       /* This is for rects */
        CCB_LAST|CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK, /* ccb_flags */
        0x00000000,     /* ccb_NextPtr */
        (CelData *) &MyCelData, /* ccb_CelData */
        0x00000000, /* ccb_PIPPtr */
        0x00000000, /* ccb_X */
        0x00000000, /* ccb_Y */
        0x00100000, /* ccb_HDX */
        0x00000000, /* ccb_HDY */
        0x00000000, /* ccb_VDX */
        0x00010000, /* ccb_VDY */
        0x00000000, /* ccb_DDX */
        0x00000000, /* ccb_DDY */
        0x1F001F00, /* ccb_PIXC */
        0x40000016, /* ccb_PRE0 */
        0x03FF1000  /* ccb_PRE1 Low bit is X control*/
    };
#endif

#if 0
void DrawARect(Word x,Word y,Word Width,Word Height,Word Color)
{
    // DC: FIXME: reimplement/replace
    #if 0
        MyCelData = Color<<16;          /* Adjust for BIG endian long to short */
        MyCCB2.ccb_XPos = (x<<16);      /* Set the topmost X */
        MyCCB2.ccb_YPos = (y<<16);      /* Set the topmost Y */
        MyCCB2.ccb_HDX = (Width<<20);   /* Set the width factor */
        MyCCB2.ccb_VDY = (Height<<16);  /* Set the height factor */
        DrawCels(VideoItem,&MyCCB2);    /* Draw the rect */
    #endif
}
#endif

//---------------------------------------------------------------------------------------------------------------------
// Draw a shape using a resource number
//---------------------------------------------------------------------------------------------------------------------
void DrawRezShape(Word x, Word y, Word RezNum) {
    DrawShape(x, y, reinterpret_cast<const CelControlBlock*>(loadResourceData(RezNum)));
    releaseResource(RezNum);
}

/**********************************

    Return the pointer of a shape from a shape array

**********************************/

const struct CelControlBlock* GetShapeIndexPtr(const void* ShapeArrayPtr, Word Index)
{
    const uint32_t* const pShapeArrayOffsets = (const uint32_t*) ShapeArrayPtr;
    const uint32_t shapeArrayOffset = byteSwappedU32(pShapeArrayOffsets[Index]);
    return (const CelControlBlock*) &((Byte*) ShapeArrayPtr)[shapeArrayOffset];
}


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
static void Timer60Hz(void)
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

LongWord ReadTick(void)
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

static LongWord TensTable[] = {
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

void LongWordToAscii(LongWord Val,Byte *AsciiPtr)
{
    Word Index;      /* Index to TensTable */
    LongWord BigNum;    /* Temp for TensTable value */
    Word Letter;        /* ASCII char */
    Word Printing;      /* Flag for printing */

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

Word SaveAFile(Byte *name,void *data,LongWord dataSize)
{
    // DC: FIXME: reimplement/replace
    #if 0
        Item fileItem;      /* Item for opened file */
        Item ioReqItem;     /* Item for i/o request */
        IOInfo ioInfo;      /* Struct for I/O request data passing */
        Err result;         /* Returned result */
        LongWord numBlocks; /* Number of blocks returned */
        LongWord blockSize; /* Size of a data block for device */
        LongWord roundedSize;   /* Rounded file size */
        FileStatus status;  /* Status I/O request record */

        DeleteFile((char *)name);   /* Get rid of the file if it was already there */

        result = CreateFile((char *)name);      /* Create the file again... */
        if (result >= 0) {
            fileItem = OpenDiskFile((char *)name);  /* Open the file for access */
            if (fileItem >= 0) {

                /* Create an IOReq to communicate with the file */

                ioReqItem = CreateIOReq(NULL,0,fileItem,0);
                if (ioReqItem >= 0) {
                    /* Get the block size of the file */
                    memset(&ioInfo, 0, sizeof(IOInfo));
                    ioInfo.ioi_Command = CMD_STATUS;
                    ioInfo.ioi_Recv.iob_Buffer = &status;
                    ioInfo.ioi_Recv.iob_Len = sizeof(FileStatus);
                    result = DoIO(ioReqItem,&ioInfo);
                    if (result >= 0) {
                        blockSize = status.fs.ds_DeviceBlockSize;
                        /* Round to block size */

                        numBlocks = (dataSize + blockSize - 1) / blockSize;

                        /* allocate the blocks we need for this file */
                        ioInfo.ioi_Command = FILECMD_ALLOCBLOCKS;
                        ioInfo.ioi_Recv.iob_Buffer = NULL;
                        ioInfo.ioi_Recv.iob_Len = 0;
                        ioInfo.ioi_Offset = numBlocks;
                        result = DoIO(ioReqItem, &ioInfo);
                        if (result >= 0) {
                            /* Tell the system how many bytes for this file */
                            memset(&ioInfo,0,sizeof(IOInfo));
                            ioInfo.ioi_Command = FILECMD_SETTYPE;
                            ioInfo.ioi_Offset = (LongWord)0x33444F46;
                            DoIO(ioReqItem,&ioInfo);

                            memset(&ioInfo,0,sizeof(IOInfo));
                            ioInfo.ioi_Command = FILECMD_SETEOF;
                            ioInfo.ioi_Offset = dataSize;
                            result = DoIO(ioReqItem, &ioInfo);
                            if (result >= 0) {
                                roundedSize = 0;
                                if (dataSize >= blockSize) {
                                    /* If we have more than one block's worth of */
                                    /* data, write as much of it as possible. */

                                    roundedSize = (dataSize / blockSize) * blockSize;
                                    ioInfo.ioi_Command = CMD_WRITE;
                                    ioInfo.ioi_Send.iob_Buffer = (void *)data;
                                    ioInfo.ioi_Send.iob_Len = roundedSize;
                                    ioInfo.ioi_Offset = 0;
                                    result = DoIO(ioReqItem, &ioInfo);

                                    data = (void *)((LongWord)data + roundedSize);
                                    dataSize -= roundedSize;    /* Data remaining */
                                }

                /* If the amount of data left isn't as large as a whole */
                /* block, we must allocate a memory buffer of the size */
                /* of the block, copy the rest of the data into it, */
                /* and write the buffer to disk. */

                                if ((result >= 0) && dataSize) {
                                    void *temp;
                                    temp = AllocAPointer(blockSize);
                                    if (temp) {
                                        memcpy(temp,data,dataSize);
                                        ioInfo.ioi_Command = CMD_WRITE;
                                        ioInfo.ioi_Send.iob_Buffer = temp;
                                        ioInfo.ioi_Send.iob_Len = blockSize;
                                        ioInfo.ioi_Offset = roundedSize;
                                        result = DoIO(ioReqItem,&ioInfo);

                                        DeallocAPointer(temp);
                                    } else {
                                        result = NOMEM;
                                    }
                                }
                            }
                        }
                    }
                    DeleteIOReq(ioReqItem);
                } else {
                    result = ioReqItem;
                }
                CloseDiskFile(fileItem);
            } else {
                result = fileItem;
            }

            /* don't leave a potentially corrupt file around... */
            if (result < 0) {
                DeleteFile((char *)name);
            }
        }
        if (result>=0) {
            result = 0;
        }
        return (result);
    #else
        return -1;
    #endif
}
