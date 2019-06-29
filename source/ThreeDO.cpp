#include "Audio/Audio.h"
#include "CelUtils.h"
#include "Doom.h"
#include "Endian.h"
#include "MathUtils.h"
#include "Mem.h"
#include "Resources.h"
#include "Textures.h"
#include "Video.h"
#include <cstdio>
#include <ctime>
#include <memory.h>
#include <SDL.h>

/* DC: headers from the 3DO SDK - leaving here for reference for now.

#include <Portfolio.h>
#include <event.h>
#include <Init3do.h>
#include <FileFunctions.h>
#include <BlockFile.h>
#include <audio.h>
#include <celutils.h>
*/

//-----------------------------------------------------------------------------
// DC: define these things from the 3DO SDK for now to fix compile errors
//-----------------------------------------------------------------------------
typedef struct { uint32_t whatever; } CelData;

typedef int32_t     Coord;
typedef void*       TagArg;

/**********************************

    This contains all the 3DO specific calls for Doom
    
**********************************/

typedef struct MyCCB {  /* Clone of the CCB Block from the 3DO includes */
    uint32_t        ccb_Flags;
    struct MyCCB*   ccb_NextPtr;
    CelData*        ccb_SourcePtr;
    void*           ccb_PLUTPtr;
    Coord           ccb_XPos;
    Coord           ccb_YPos;
    int32_t         ccb_HDX;
    int32_t         ccb_HDY;
    int32_t         ccb_VDX;
    int32_t         ccb_VDY;
    int32_t         ccb_HDDX;
    int32_t         ccb_HDDY;
    uint32_t        ccb_PIXC;
    uint32_t        ccb_PRE0;
    uint32_t        ccb_PRE1;
} MyCCB;    /* I DON'T include width and height */

#define LIGHTSCALESHIFT 3
#if 0
    0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,    /* 1/16 - 8/16 */
    0x00D0,0x1300,0x08D0,0x1700,0x10D0,0x1B00,0x18D0,0x1F00,
#endif

static Word LightTable[] = {
    0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,    /* 1/16 - 8/16 */
    0x00D0,0x00D0,0x1300,0x1300,0x08D0,0x08D0,0x1700,0x1700,
    0x10D0,0x10D0,0x1B00,0x1B00,0x18D0,0x18D0,0x1F00,0x1F00,
    0x1F00,0x1F00,0x1F00,0x1F00,0x1F00,0x1F00,0x1F00,0x1F00,
};

static void FlushCCBs(void);

// DC: TODO: unused currently
#if 0
static void WipeDoom(LongWord *OldScreen,LongWord *NewScreen);
#endif

extern void DrawASpan(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep,
    Byte *Dest);

#define CCBTotal 0x200

static MyCCB CCBArray[CCBTotal];            /* Array of CCB structs */

// DC: TODO: unused currently
#if 0
static MyCCB *CurrentCCB = &CCBArray[0];    /* Pointer to empty CCB */
static LongWord LastTicCount;               /* Time mark for page flipping */
#endif

LongWord LastTics;                          /* Time elapsed since last page flip */
Word WorkPage;                              /* Which frame is not being displayed */

// DC: TODO: unused currently
#if 0
static Byte *CelLine190;
#endif

Byte SpanArray[MAXSCREENWIDTH*MAXSCREENHEIGHT]; /* Buffer for floor textures */
Byte *SpanPtr = SpanArray;      /* Pointer to empty buffer */

#define SKYSCALE(x) (Fixed)(1048576.0*(x/160.0))

// DC: TODO: unused currently
#if 0
static Fixed SkyScales[6] = {
    SKYSCALE(160.0),
    SKYSCALE(144.0),
    SKYSCALE(128.0),
    SKYSCALE(112.0),
    SKYSCALE(96.0),
    SKYSCALE(80.0)
};
#endif

#define SCREENS 3                   /* Need to page flip */
uint32_t MainTask;                  /* My own task item */

// DC: TODO: unused currently
#if 0
static uint32_t ScreenPageCount;    /* Number of screens */
static Item ScreenItems[SCREENS];   /* Referances to the game screens */
static Item VideoItems[SCREENS];
static long ScreenByteCount;        /* How many bytes for each screen */
static Item ScreenGroupItem = 0;    /* Main screen referance */
static Byte *ScreenMaps[SCREENS];   /* Pointer to the bitmap screens */
static Item VRAMIOReq;              /* I/O Request for screen copy */
#endif

/**********************************

    Run an external program and wait for compleation

**********************************/

// DC: 3DO specific - disable
#if 0
static void RunAProgram(char *ProgramName)
{
    Item LogoItem;
    LogoItem=LoadProgram(ProgramName);  /* Load and begin execution */
    do {
        Yield();                        /* Yield all CPU time to the other program */
    } while (LookupItem(LogoItem));     /* Wait until the program quits */
    DeleteItem(LogoItem);               /* Dispose of the 3DO logo code */
}
#endif

/**********************************

    Init the 3DO variables to a specific screen

**********************************/

static void SetMyScreen(Word Page)
{
    // DC: FIXME: implement/replace
    #if 0
        VideoItem = VideoItems[Page];           /* Get the bitmap item # */
        VideoScreen = ScreenItems[Page];
        VideoPointer = (Byte *) &ScreenMaps[Page][0];
        CelLine190 = (Byte *) &VideoPointer[190*640];
    #endif
}

/**********************************

    Init the system tools

    Start up all the tools for the 3DO system
    Return TRUE if all systems are GO!

**********************************/

// DC: TODO: unused currently
#if 0
static Word HeightArray[1] = { 200 };   /* I want 200 lines for display memory */
#endif

// DC: 3DO specific - disable
#if 0
    static Word MyCustomVDL[] = {
        VDL_RELSEL|         /* Relative pointer to next VDL */
        (1<<VDL_LEN_SHIFT)|     /* (DMA) 1 control words in this VDL entry */
        (20<<VDL_LINE_SHIFT),   /* Scan lines to persist */
        0,              /* Current video buffer */
        0,              /* Previous video buffer */
        4*4,            /* Pointer to next vdl */
        0xE0000000,     /* Set the screen to BLACK */
        VDL_NOP,        /* Filler to align to 16 bytes */
        VDL_NOP,
        VDL_NOP,

        VDL_RELSEL|
        VDL_ENVIDDMA|               /* Enable video DMA */
        VDL_LDCUR|                  /* Load current address */
        VDL_LDPREV|                 /* Load previous address */
        ((32+2)<<VDL_LEN_SHIFT)|            /* (DMA) 2 control words in this VDL entry */
        (198<<VDL_LINE_SHIFT),      /* Scan lines to persist */
        0,              /* Current video buffer */
        0,              /* Previous video buffer */
        (32+4)*4,           /* Pointer to next vdl */

        VDL_DISPCTRL|   /* Video display control word */
        VDL_CLUTBYPASSEN|   /* Allow fixed clut */
        VDL_WINBLSB_BLUE|   /* Normal blue */
    #if 1
        VDL_WINVINTEN|      /* Enable HV interpolation */
        VDL_WINHINTEN|
        VDL_VINTEN|
        VDL_HINTEN|
    #endif
        VDL_BLSB_BLUE|      /* Normal */
        VDL_HSUB_FRAME,     /* Normal */
        0x00000000,     /* Default CLUT */
        0x01080808,
        0x02101010,
        0x03181818,
        0x04202020,
        0x05292929,
        0x06313131,
        0x07393939,
        0x08414141,
        0x094A4A4A,
        0x0A525252,
        0x0B5A5A5A,
        0x0C626262,
        0x0D6A6A6A,
        0x0E737373,
        0x0F7B7B7B,
        0x10838383,
        0x118B8B8B,
        0x12949494,
        0x139C9C9C,
        0x14A4A4A4,
        0x15ACACAC,
        0x16B4B4B4,
        0x17BDBDBD,
        0x18C5C5C5,
        0x19CDCDCD,
        0x1AD5D5D5,
        0x1BDEDEDE,
        0x1CE6E6E6,
        0x1DEEEEEE,
        0x1EF6F6F6,
        0x1FFFFFFF,
        0xE0000000,     /* Set the screen to BLACK */
        VDL_NOP,                /* Filler to align to 16 bytes */
        VDL_NOP,

        (1<<VDL_LEN_SHIFT)|         /* (DMA) 1 control words in this VDL entry */
        (0<<VDL_LINE_SHIFT),        /* Scan lines to persist (Forever) */
        0,              /* Current video buffer */
        0,              /* Previous video buffer */
        0,              /* Pointer to next vdl (None) */
        0xE0000000,     /* Set the screen to BLACK */
        VDL_NOP,                /* Filler to align to 16 bytes */
        VDL_NOP,
        VDL_NOP
    };

    static TagArg ScreenTags[] =    {       /* Change this to change the screen count! */
        CSG_TAG_SPORTBITS, (void *)0,   /* Allow SPORT DMA (Must be FIRST) */
        CSG_TAG_SCREENCOUNT, (void *)SCREENS,   /* How many screens to make! */
        CSG_TAG_BITMAPCOUNT,(void *)1,
        CSG_TAG_BITMAPHEIGHT_ARRAY,(void *)&HeightArray[0],
        CSG_TAG_DONE, 0         /* End of list */
    };
#endif

// DC: TODO: unused currently
#if 0
static char FileName[32];
#endif

void InitTools(void)
{
    Word i;     /* Temp */

    // DC: 3DO specific - disabling
    #if 0
        long width, height;         /* Screen width & height */
        struct Screen *screen;      /* Pointer to screen info */
        struct ItemNode *Node;
        Item MyVDLItem;             /* Read page PRF-85 for info */
    
        #if 1
            Show3DOLogo();              /* Show the 3DO Logo */
            RunAProgram("IdLogo IDLogo.cel");
        #if 1           /* Set to 1 for Japanese version */
            RunAProgram("IdLogo LogicLogo.cel");
            RunAProgram("PlayMovie EALogo.cine");
            RunAProgram("IdLogo AdiLogo.cel");
        #else
            RunAProgram("PlayMovie Logic.cine");
            RunAProgram("PlayMovie AdiLogo.cine");
        #endif
        #endif
    
        if (OpenGraphicsFolio() ||  /* Start up the graphics system */
            (OpenAudioFolio()<0) ||     /* Start up the audio system */
            (OpenMathFolio()<0) )
        {
        FooBar:
            exit(10);
        }
    #endif
    
    #if 0 /* Set to 1 for the PAL version, 0 for the NTSC version */
        QueryGraphics(QUERYGRAF_TAG_DEFAULTDISPLAYTYPE,&width);
        if (width==DI_TYPE_NTSC) {
            goto FooBar();
        }
    #endif

    #if 0 /* Remove for final build! */
        ChangeDirectory("/CD-ROM");
    #endif

    // DC: 3DO specific code - disabling
    #if 0
        ScreenTags[0].ta_Arg = (void *)GETBANKBITS(GrafBase->gf_ZeroPage);
        ScreenGroupItem = CreateScreenGroup(ScreenItems,ScreenTags);

        if (ScreenGroupItem<0) {        /* Error creating screens? */
            goto FooBar;
        }
        AddScreenGroup(ScreenGroupItem,NULL);       /* Add my screens to the system */

        screen = (Screen*)LookupItem(ScreenItems[0]);
        if (!screen) {
            goto FooBar;
        }

        width = screen->scr_TempBitmap->bm_Width;       /* How big is the screen? */
        height = screen->scr_TempBitmap->bm_Height;

        ScreenPageCount = (width*2*height+GrafBase->gf_VRAMPageSize-1)/GrafBase->gf_VRAMPageSize;
        ScreenByteCount = ScreenPageCount * GrafBase->gf_VRAMPageSize;

        i=0;
        do {        /* Process the screens */
            screen = (Screen *)LookupItem(ScreenItems[i]);
            ScreenMaps[i] = (Byte *)screen->scr_TempBitmap->bm_Buffer;
            memset(ScreenMaps[i],0,ScreenByteCount);    /* Clear the screen */
            Node = (ItemNode *) screen->scr_TempBitmap; /* Get the bitmap pointer */
            VideoItems[i] = (Item)Node->n_Item;         /* Get the bitmap item # */
            MyCustomVDL[9]=MyCustomVDL[10] = (Word)ScreenMaps[i];
            MyVDLItem = SubmitVDL((VDLEntry *)&MyCustomVDL[0],sizeof(MyCustomVDL)/4,VDLTYPE_FULL);
            SetVDL(ScreenItems[i],MyVDLItem);

            SetClipWidth(VideoItems[i],320);
            SetClipHeight(VideoItems[i],200);       /* I only want 200 lines */
            SetClipOrigin(VideoItems[i],0,0);       /* Set the clip top for the screen */
        } while (++i<SCREENS);

        InitEventUtility(1, 1, false);    /* I want 1 joypad, 1 mouse, and passive listening */
    #endif

    audioInit();

    // DC: 3DO specific code - disabling
    #if 0
        MainTask = KernelBase->kb_CurrentTask->t.n_Item;    /* My task Item */
        VRAMIOReq = GetVRAMIOReq();
        SetMyScreen(0);                                     /* Init the video display */
    #endif

    audioLoadAllSounds();
    resourcesInit();
    
    {
        MyCCB *CCBPtr;
        i = CCBTotal;
        CCBPtr = CCBArray;
        
        do {
            CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);   /* Create the next offset */
            CCBPtr->ccb_HDDX = 0;   /* Set the defaults */
            CCBPtr->ccb_HDDY = 0;
            ++CCBPtr;
        } while (--i);
    }
}

/**********************************

    Read a file from NVRAM or a memory card into buf

**********************************/

// DC: 3DO specific - remove eventually
#if 0
    static LongWord RamFileSize;
#endif

int32_t StdReadFile(char *fName,char *buf)
{
    // DC: FIXME: Implement File I/O
    #if 0
        int32 err;          /* Error code to return */
        Item fd;            /* Disk file referance */
        Item req;           /* IO request item */
        IOReq *reqp;        /* Pointer to IO request item */
        IOInfo params;      /* Parameter list for I/O information */
        DeviceStatus ds;    /* Struct for device status */
    
        fd = OpenDiskFile(fName);   /* Open the file */
        if (fd < 0) {               /* Error? */
            return fd;
        }
    
        req = CreateIOReq(NULL,0,fd,0);         /* Create an I/O item */
        reqp = (IOReq *)LookupItem(req);        /* Deref the item pointer */
        memset(&params,0,sizeof(IOInfo));       /* Blank the I/O record */
        memset(&ds,0,sizeof(DeviceStatus));     /* Blank the device status */
        params.ioi_Command = CMD_STATUS;        /* Make a status command */
        params.ioi_Recv.iob_Buffer = &ds;       /* Set the I/O buffer ptr */
        params.ioi_Recv.iob_Len = sizeof(DeviceStatus); /* Set the length */
        err = DoIO(req,&params);                /* Perform the status I/O */
        if (err>=0) {           /* Status ok? */
            /* Try to read it in */

            /* Calc the read size based on blocks */
            RamFileSize = ds.ds_DeviceBlockCount * ds.ds_DeviceBlockSize;
            memset(&params,0,sizeof(IOInfo));       /* Zap the I/O info record */
            params.ioi_Command = CMD_READ;          /* Read command */
            params.ioi_Recv.iob_Len = RamFileSize;  /* Data length */
            params.ioi_Recv.iob_Buffer = buf;       /* Data buffer */
            err = DoIO(req,&params);                /* Read the file */
        }
        DeleteIOReq(req);       /* Release the IO request */
        CloseDiskFile(fd);      /* Close the disk file */
        return err;             /* Return the error code (If any) */
    #else
        return -1;
    #endif
}

/**********************************

    Write out the prefs to the NVRAM

**********************************/

#define PREFWORD 0x4C57
static char PrefsName[] = "/NVRAM/DoomPrefs";       /* Save game name */

void WritePrefsFile(void)
{
    Word PrefFile[10];      /* Must match what's in ReadPrefsFile!! */
    Word CheckSum;          /* Checksum total */
    Word i;

    PrefFile[0] = PREFWORD;
    PrefFile[1] = StartSkill;
    PrefFile[2] = StartMap;
    PrefFile[3] = audioGetSoundVolume();
    PrefFile[4] = audioGetMusicVolume();
    PrefFile[5] = ControlType;
    PrefFile[6] = MaxLevel;
    PrefFile[7] = ScreenSize;
    PrefFile[8] = 0;            // Was 'LowDetail' - now unused
    PrefFile[9] = 12345;        /* Init the checksum */
    i = 0;
    CheckSum = 0;
    do {
        CheckSum += PrefFile[i];        /* Make a simple checksum */
    } while (++i<10);
    PrefFile[9] = CheckSum;
    SaveAFile((Byte *)PrefsName,&PrefFile,sizeof(PrefFile));    /* Save the game file */
}

//-------------------------------------------------------------------------------------------------
// Clear out the prefs file
//-------------------------------------------------------------------------------------------------
void ClearPrefsFile() {
    StartSkill = sk_medium;                     // Init the basic skill level
    StartMap = 1;                               // Only allow playing from map #1
    audioSetSoundVolume(MAX_AUDIO_VOLUME);      // Init the sound effects volume
    audioSetMusicVolume(MAX_AUDIO_VOLUME);      // Init the music volume
    ControlType = 3;                            // Use basic joypad controls
    MaxLevel = 24;                              // Only allow level 1 to select from
    ScreenSize = 0;                             // Default screen size
    WritePrefsFile();                           // Output the new prefs
}

/**********************************

    Load in the standard prefs

**********************************/

void ReadPrefsFile(void)
{
    Word PrefFile[88];      /* Must match what's in WritePrefsFile!! */
    Word CheckSum;          /* Running checksum */
    Word i;

    if (StdReadFile(PrefsName,(char *) PrefFile)<0) {   /* Error reading? */
        ClearPrefsFile();       /* Clear it out */
        return;
    }

    i = 0;
    CheckSum = 12345;       /* Init the checksum */
    do {
        CheckSum+=PrefFile[i];  /* Calculate the checksum */
    } while (++i<9);

    if ((CheckSum != PrefFile[10-1]) || (PrefFile[0] !=PREFWORD)) {
        ClearPrefsFile();   /* Bad ID or checksum! */
        return;
    }
    
    StartSkill = (skill_t)PrefFile[1];
    StartMap = PrefFile[2];
    audioSetSoundVolume(PrefFile[3]);
    audioSetMusicVolume(PrefFile[4]);
    ControlType = PrefFile[5];
    MaxLevel = PrefFile[6];
    ScreenSize = PrefFile[7];
    
    if ((StartSkill >= (sk_nightmare+1)) ||
        (StartMap >= 27) ||
        (ControlType >= 6) ||
        (MaxLevel >= 26) ||
        (ScreenSize >= 6)
    ) {
        ClearPrefsFile();
    }
}

/**********************************

    Flush all the cached CCB's

**********************************/

static void FlushCCBs(void)
{
    // DC: 3DO specific code - disabling
    #if 0
        MyCCB* NewCCB;

        NewCCB = CurrentCCB;
        if (NewCCB!=&CCBArray[0]) {
            --NewCCB;       /* Get the last used CCB */
            NewCCB->ccb_Flags |= CCB_LAST;  /* Mark as the last one */
            DrawCels(VideoItem,(CCB *)&CCBArray[0]);    /* Draw all the cels in one shot */
            CurrentCCB = &CCBArray[0];      /* Reset the empty entry */
        }
        SpanPtr = SpanArray;        /* Reset the floor texture pointer */
    #endif
}

/**********************************

    Display the current framebuffer
    If < 1/15th second has passed since the last display, busy wait.
    15 fps is the maximum frame rate, and any faster displays will
    only look ragged.

**********************************/

void UpdateAndPageFlip(const bool bAllowDebugClear) {
    Video::present(bAllowDebugClear);

    // DC: FIXME: implement/replace
    #if 0
        LongWord NewTick;

        FlushCCBs();
        if (DoWipe) {
            Word PrevPage;
            void *NewImage;
            void *OldImage;
            
            DoWipe = false;
            NewImage = VideoPointer;    /* Pointer to the NEW image */
            PrevPage = WorkPage-1;  /* Get the currently displayed page */
            if (PrevPage==-1) {     /* Wrapped? */
                PrevPage = SCREENS-1;
            }
            SetMyScreen(PrevPage);      /* Set videopointer to display buffer */
            if (!PrevPage) {
                PrevPage=SCREENS;
            }
            --PrevPage;
            OldImage = (Byte *) &ScreenMaps[PrevPage][0];   /* Get work buffer */
            
                /* Copy the buffer from display to work */
            memcpy(OldImage,VideoPointer,320*200*2);
            WipeDoom((LongWord *)OldImage,(LongWord *)NewImage);            /* Perform the wipe */
        }
        DisplayScreen(ScreenItems[WorkPage],0);     /* Display the hidden page */
        if (++WorkPage>=SCREENS) {      /* Next screen in line */
            WorkPage = 0;
        }
        SetMyScreen(WorkPage);      /* Set the 3DO vars */
        do {
            NewTick = ReadTick();               /* Get the time mark */
            LastTics = NewTick - LastTicCount;  /* Get the time elapsed */
        } while (!LastTics);                    /* Hmmm, too fast?!?!? */
        LastTicCount = NewTick;                 /* Save the time mark */
    #endif
}

/**********************************

    Draw a shape centered on the screen
    Used for "Loading or Paused" pics

**********************************/

void DrawPlaque(Word RezNum)
{
    Word PrevPage;
    PrevPage = WorkPage-1;
    if (PrevPage==-1) {
        PrevPage = SCREENS-1;
    }
    FlushCCBs();        /* Flush pending draws */
    SetMyScreen(PrevPage);      /* Draw to the active screen */
    const CelControlBlock* const pPic = (const CelControlBlock*) loadResourceData(RezNum);
    DrawShape(160 - (getCCBWidth(pPic) / 2), 80, pPic);
    FlushCCBs();        /* Make sure it's drawn */
    releaseResource(RezNum);
    SetMyScreen(WorkPage);      /* Reset to normal */
}

//---------------------------------------------------------------------------------------------------------------------
// Main entry point for 3DO
//---------------------------------------------------------------------------------------------------------------------
void ThreeDOMain() {
    InitTools();                // Init the 3DO tool system
    Video::init();
    UpdateAndPageFlip(true);    // Init the video display's vars    
    ReadPrefsFile();            // Load defaults
    D_DoomMain();               // Start doom
    Video::shutdown();
}

/**********************************

    Draw a scaled solid line

**********************************/

void AddCCB(Word x,Word y,MyCCB* NewCCB)
{
    // DC: FIXME: implement/replace
    #if 0
        MyCCB* DestCCB;         /* Pointer to new CCB entry */
        LongWord TheFlags;      /* CCB flags */
        LongWord ThePtr;        /* Temp pointer to munge */

        DestCCB = CurrentCCB;       /* Copy pointer to local */
        if (DestCCB>=&CCBArray[CCBTotal]) {     /* Am I full already? */
            FlushCCBs();
            DestCCB = CCBArray;
        }
        TheFlags = NewCCB->ccb_Flags;       /* Preload the CCB flags */
        DestCCB->ccb_XPos = x<<16;      /* Set the x and y coord */
        DestCCB->ccb_YPos = y<<16;
        DestCCB->ccb_HDX = NewCCB->ccb_HDX; /* Set the data for the CCB */
        DestCCB->ccb_HDY = NewCCB->ccb_HDY;
        DestCCB->ccb_VDX = NewCCB->ccb_VDX;
        DestCCB->ccb_VDY = NewCCB->ccb_VDY;
        DestCCB->ccb_PIXC = NewCCB->ccb_PIXC;
        DestCCB->ccb_PRE0 = NewCCB->ccb_PRE0;
        DestCCB->ccb_PRE1 = NewCCB->ccb_PRE1;

        ThePtr = (LongWord)NewCCB->ccb_SourcePtr;   /* Force absolute address */
        if (!(TheFlags&CCB_SPABS)) {
            ThePtr += ((LongWord)NewCCB)+12;    /* Convert relative to abs */
        }
        DestCCB->ccb_SourcePtr = (CelData *)ThePtr; /* Save the source ptr */

        if (TheFlags&CCB_LDPLUT) {      /* Only load a new plut if REALLY needed */
            ThePtr = (LongWord)NewCCB->ccb_PLUTPtr;
            if (!(TheFlags&CCB_PPABS)) {        /* Relative plut? */
                ThePtr += ((LongWord)NewCCB)+16;    /* Convert relative to abs */
            }
            DestCCB->ccb_PLUTPtr = (void *)ThePtr;  /* Save the PLUT pointer */
        }
        DestCCB->ccb_Flags = (TheFlags & ~(CCB_LAST|CCB_NPABS)) | (CCB_SPABS|CCB_PPABS);
        ++DestCCB;              /* Next CCB */
        CurrentCCB = DestCCB;   /* Save the CCB pointer */
    #endif
}

static void DrawShapeImpl(const uint32_t x1, const uint32_t y1, const CelControlBlock* const pShape, bool bIsMasked) {
    // TODO: DC - This is temp!
    uint16_t* pImage;
    uint16_t imageW;
    uint16_t imageH;
    decodeDoomCelSprite(pShape, &pImage, &imageW, &imageH);
    
    const uint32_t xEnd = x1 + imageW;
    const uint32_t yEnd = y1 + imageH;
    const uint16_t* pCurImagePixel = pImage;

    for (uint32_t y = y1; y < yEnd; ++y) {
        for (uint32_t x = x1; x < xEnd; ++x) {
            if (x >= 0 && x < Video::SCREEN_WIDTH) {
                if (y >= 0 && y < Video::SCREEN_HEIGHT) {
                    const uint16_t color = *pCurImagePixel;
                    const uint16_t colorA = (color & 0b1000000000000000) >> 10;
                    const uint16_t colorR = (color & 0b0111110000000000) >> 10;
                    const uint16_t colorG = (color & 0b0000001111100000) >> 5;
                    const uint16_t colorB = (color & 0b0000000000011111) >> 0;

                    bool bTransparentPixel = false;

                    if (bIsMasked) {
                        bTransparentPixel = ((color & 0x7FFF) == 0);
                    }

                    if (colorA == 0) {
                        bTransparentPixel = true;
                    }

                    if (!bTransparentPixel) {
                        const uint32_t finalColor = (
                            (colorR << 27) |
                            (colorG << 19) |
                            (colorB << 11) |
                            255
                        );

                        Video::gFrameBuffer[y * Video::SCREEN_WIDTH + x] = finalColor;
                    }
                }
            }

            ++pCurImagePixel;
        }
    }

    MemFree(pImage);
}

/**********************************

    Draw a masked shape on the screen

**********************************/

void DrawMShape(const uint32_t x1, const uint32_t y1, const CelControlBlock* const pShape) {
    // TODO: DC - This is temp!
    DrawShapeImpl(x1, y1, pShape, true);

    // DC: FIXME: implement/replace
    #if 0
        ((MyCCB*)ShapePtr)->ccb_Flags &= ~CCB_BGND;     /* Enable masking */
        AddCCB(x,y,(MyCCB*)ShapePtr);                   /* Draw the shape */
    #endif
}

/**********************************

    Draw an unmasked shape on the screen

**********************************/

void DrawShape(const uint32_t x1, const uint32_t y1, const CelControlBlock* const pShape) {
    // TODO: DC - This is temp!
    DrawShapeImpl(x1, y1, pShape, false);

    // DC: FIXME: implement/replace
    #if 0
        ((MyCCB*)ShapePtr)->ccb_Flags |= CCB_BGND;      /* Disable masking */
        AddCCB(x,y,(MyCCB*)ShapePtr);                   /* Draw the shape */
    #endif
}

/**********************************

    This code is functionally equivalent to the Burgerlib
    version except that it is using the cached CCB system.

**********************************/

void DrawARect(Word x1, Word y1, Word Width, Word Height, Word color) {

    

    // DC: FIXME: implement/replace
    #if 0
        MyCCB* DestCCB;         /* Pointer to new CCB entry */

        DestCCB = CurrentCCB;       /* Copy pointer to local */
        if (DestCCB>=&CCBArray[CCBTotal]) {     /* Am I full already? */
            FlushCCBs();                /* Draw all the CCBs/Lines */
            DestCCB=CCBArray;
        }
        DestCCB->ccb_Flags = CCB_LDSIZE|CCB_LDPRS|
            CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
            CCB_ACE|CCB_BGND|CCB_NOBLK; /* ccb_flags */

        DestCCB->ccb_PIXC = 0x1F00;     /* PIXC control */
        DestCCB->ccb_PRE0 = 0x40000016;     /* Preamble */
        DestCCB->ccb_PRE1 = 0x03FF1000;     /* Second preamble */
        DestCCB->ccb_SourcePtr = (CelData *)0;  /* Save the source ptr */
        DestCCB->ccb_PLUTPtr = (void *)(color<<16);     /* Set the color pixel */
        DestCCB->ccb_XPos = x1<<16;     /* Set the x and y coord for start */
        DestCCB->ccb_YPos = y1<<16;
        DestCCB->ccb_HDX = Width<<20;       /* OK */
        DestCCB->ccb_HDY = 0<<20;
        DestCCB->ccb_VDX = 0<<16;
        DestCCB->ccb_VDY = Height<<16;
        ++DestCCB;              /* Next CCB */
        CurrentCCB = DestCCB;   /* Save the CCB pointer */
    #endif
}

/**********************************

    Ok boys amd girls, the follow functions are drawing primitives
    for Doom 3DO that take into account the fact that I draw all lines from
    back to front. This way I use painter's algorithm to do clipping since the
    3DO cel engine does not allow arbitrary clipping of any kind.
    
    On the first pass, I draw all the sky lines without any regard for clipping
    since no object can be behind the sky.
    
    Next I then sort the sprites from back to front.
    
    Then I draw the walls from back to front, mixing in all the sprites so
    that all shapes will clip properly using painter's algorithm.

**********************************/

#define MAX_WALL_LIGHT_VALUE 15
#define MAX_FLOOR_LIGHT_VALUE 15
#define MAX_SPRITE_LIGHT_VALUE 31

/**********************************

    Drawing the sky is the easiest, so I'll do this first.
    The parms are, 
    tx_x = screen x coord for the virtual screen.
    colnum = index for which scan line to draw from the source image. Note that
        this number has all bits set so I must mask off the unneeded bits for
        texture wraparound.
        
    No light shading is used for the sky. The scale factor is a constant.
    
**********************************/

extern Word tx_x;
extern int tx_scale;

void DrawSkyLine()
{
    // Note: sky textures are 256 pixels wide so this wraps around
    const uint32_t colNum = (((xtoviewangle[tx_x] + viewangle) >> ANGLETOSKYSHIFT) & 0xFF);
    
    // Sky is always rendered at max light and 1.0 scale
    tx_texturelight = MAX_WALL_LIGHT_VALUE << LIGHTSCALESHIFT;
    tx_scale = 1 << SCALEBITS;
    
    // Set source texture details
    // TODO: don't keep doing this for each column
    const Texture* const pTexture = (const Texture*) getWallTexture(getCurrentSkyTexNum());
    const uint32_t texHeight = pTexture->height;
    DrawWallColumn(0, colNum * texHeight, 0, texHeight, (Byte*) pTexture->pData, texHeight);
    
    // DC: FIXME: implement/replace
    #if 0
        Byte *Source;
        Word Colnum;
        MyCCB* DestCCB;         /* Pointer to new CCB entry */
    
        DestCCB = CurrentCCB;       /* Copy pointer to local */
        if (DestCCB>=&CCBArray[CCBTotal]) {     /* Am I full already? */
            FlushCCBs();                /* Draw all the CCBs/Lines */
            DestCCB = CCBArray;
        }
        Colnum = (((xtoviewangle[tx_x]+viewangle)>>ANGLETOSKYSHIFT)&0xFF)*64;
        Source = (Byte *)(*SkyTexture->data);   /* Index to the true shape */

        DestCCB[0].ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT;    /* ccb_flags */
        DestCCB[0].ccb_PRE0 = 0x03;
        DestCCB[0].ccb_PRE1 = 0x3E005000|(128-1);   /* Project the pixels */
        DestCCB[0].ccb_PLUTPtr = Source;        /* Get the palette ptr */
        DestCCB[0].ccb_SourcePtr = (CelData *)&Source[Colnum+32];   /* Get the source ptr */
        DestCCB[0].ccb_XPos = tx_x<<16;     /* Set the x and y coord for start */
        DestCCB[0].ccb_YPos = 0<<16;
        DestCCB[0].ccb_HDX = 0<<20;     /* Convert 6 bit frac to CCB scale */
        DestCCB[0].ccb_HDY = SkyScales[ScreenSize]; /* Video stretch factor */
        DestCCB[0].ccb_VDX = 1<<16;
        DestCCB[0].ccb_VDY = 0<<16;
        DestCCB[0].ccb_PIXC = 0x1F00;       /* PIXC control */
        ++DestCCB;          /* Next CCB */
        CurrentCCB = DestCCB;   /* Save the CCB pointer */
    #endif
}

//---------------------------------------------------------------------------------------------------------------------
// Makes a framebuffer color from the given RGB values.
// Saturates/clamps the values if they are out of range.
//---------------------------------------------------------------------------------------------------------------------
static uint32_t makeFramebufferColor(const Fixed rFrac, const Fixed gFrac, const Fixed bFrac) {
    const uint16_t rInt = rFrac >> (FRACBITS - 3);
    const uint16_t gInt = gFrac >> (FRACBITS - 3);
    const uint16_t bInt = bFrac >> (FRACBITS - 3);
    const uint32_t rClamp = (rInt > 0xFF) ? 0xFF : rInt;
    const uint32_t gClamp = (gInt > 0xFF) ? 0xFF : gInt;
    const uint32_t bClamp = (bInt > 0xFF) ? 0xFF : bInt;
    return ((rClamp << 24) | (gClamp << 16) | (bClamp << 8) | 0xFF);
}

//---------------------------------------------------------------------------------------------------------------------
// Returns a fixed point multipler for the given texture light value (which is in 4.3 format)
// This can be used to scale RGB values due to lighting.
//---------------------------------------------------------------------------------------------------------------------
static Fixed getLightMultiplier(const uint32_t lightValue, const uint32_t maxLightValue) {
    const Fixed maxLightValueFrac = int32ToSFixed16_16(maxLightValue);
    const Fixed textureLightFrac = lightValue << (FRACBITS - LIGHTSCALESHIFT);
    const Fixed lightMultiplier = sfixedDiv16_16(textureLightFrac, maxLightValueFrac);
    return (lightMultiplier > FRACUNIT) ? FRACUNIT : lightMultiplier;
}

/**********************************

    Drawing the wall columns are a little trickier, so I'll do this next.
    The parms are, 
    tx_x = screen x coord for the virtual screen.
    y = screen y coord for the virtual screen.
    bottom = screen y coord for the BOTTOM of the pixel run. Subtract from top
        to get the exact destination pixel run count.
    colnum = index for which scan line to draw from the source image. Note that
        this number has all bits set so I must mask off the unneeded bits for
        texture wraparound.
        
    No light shading is used. The scale factor is a constant.
    
**********************************/

void DrawWallColumn(
    const Word y,
    const Word Colnum,
    const Word ColY,
    const Word TexHeight,
    const Byte* const Source,
    const Word Run
)
{
    // TODO: TEMP - CLEANUP
    const Word numPixels = (Run * tx_scale) >> SCALEBITS;
    const Word numPixelsRounded = ((Run * tx_scale) & 0x1F0) != 0 ? numPixels + 1 : numPixels;    
    const Fixed lightMultiplier = getLightMultiplier(tx_texturelight, MAX_WALL_LIGHT_VALUE);

    const uint16_t* const pPLUT = (const uint16_t*) Source;

    for (uint32_t pixNum = 0; pixNum < numPixelsRounded; ++pixNum) {
        const uint32_t dstY = y + pixNum;
        if (dstY >= 0 && dstY < ScreenHeight) {
            const uint32_t pixTexYOffsetFixed = (pixNum << (SCALEBITS + 1)) / tx_scale;
            const uint32_t pixTexYOffset = pixTexYOffsetFixed & 1 ? (pixTexYOffsetFixed / 2) + 1 : pixTexYOffsetFixed / 2;
            
            const uint32_t texYOffset = (ColY + pixTexYOffset) % (TexHeight);
            const uint32_t texOffset = Colnum + texYOffset;
            
            const uint8_t colorByte = Source[32 + texOffset / 2];
            const uint8_t colorIdx = (texOffset & 1) != 0 ? (colorByte & 0x0F) : (colorByte & 0xF0) >> 4;
            
            const uint16_t color = byteSwappedU16(pPLUT[colorIdx]);
            const uint16_t texR = (color & 0b0111110000000000) >> 10;
            const uint16_t texG = (color & 0b0000001111100000) >> 5;
            const uint16_t texB = (color & 0b0000000000011111) >> 0;
            
            const Fixed texRFrac = int32ToSFixed16_16(texR);
            const Fixed texGFrac = int32ToSFixed16_16(texG);
            const Fixed texBFrac = int32ToSFixed16_16(texB);
            const Fixed darkenedR = sfixedMul16_16(texRFrac, lightMultiplier);
            const Fixed darkenedG = sfixedMul16_16(texGFrac, lightMultiplier);
            const Fixed darkenedB = sfixedMul16_16(texBFrac, lightMultiplier);

            const uint32_t finalColor = makeFramebufferColor(darkenedR, darkenedG, darkenedB);
            const uint32_t screenX = tx_x + ScreenXOffset;
            const uint32_t screenY = dstY + ScreenYOffset;

            Video::gFrameBuffer[screenY * Video::SCREEN_WIDTH + screenX] = finalColor;
        }
    }
    
    // DC: FIXME: implement/replace
    #if 0
        MyCCB* DestCCB;         /* Pointer to new CCB entry */
        Word Colnum7;
    
        DestCCB = CurrentCCB;       /* Copy pointer to local */
        if (DestCCB>=&CCBArray[CCBTotal]) {     /* Am I full already? */
            FlushCCBs();                /* Draw all the CCBs/Lines */
            DestCCB = CCBArray;
        }

        Colnum7 = Colnum & 7;   /* Get the pixel skip */
        Colnum = Colnum>>1;     /* Pixel to byte offset */
        Colnum += 32;           /* Index past the PLUT */
        Colnum &= ~3;           /* Long word align the source */
        DestCCB[0].ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT|CCB_USEAV;  /* ccb_flags */
        DestCCB[0].ccb_PRE0 = (Colnum7<<24)|0x03;
        DestCCB[0].ccb_PRE1 = 0x3E005000|(Colnum7+Run-1);   /* Project the pixels */
        DestCCB[0].ccb_PLUTPtr = Source;        /* Get the palette ptr */
        DestCCB[0].ccb_SourcePtr = (CelData *)&Source[Colnum];  /* Get the source ptr */
        DestCCB[0].ccb_XPos = tx_x<<16;     /* Set the x and y coord for start */
        DestCCB[0].ccb_YPos = (y<<16)+0xFF00;
        DestCCB[0].ccb_HDX = 0<<20;     /* Convert 6 bit frac to CCB scale */
        DestCCB[0].ccb_HDY = (tx_scale<<11);
        DestCCB[0].ccb_VDX = 1<<16;
        DestCCB[0].ccb_VDY = 0<<16;
        DestCCB[0].ccb_PIXC = LightTable[tx_texturelight>>LIGHTSCALESHIFT];     /* PIXC control */
    
        ++DestCCB;              /* Next CCB */
        CurrentCCB = DestCCB;   /* Save the CCB pointer */
    #endif
}

/**********************************

    Drawing the floor and ceiling is the hardest, so I'll do this last.
    The parms are, 
    x = screen x coord for the virtual screen. Must be offset
        to the true screen coords.
    top = screen y coord for the virtual screen. Must also be offset
        to the true screen coords.
    bottom = screen y coord for the BOTTOM of the pixel run. Subtract from top
        to get the exact destination pixel run count.
    colnum = index for which scan line to draw from the source image. Note that
        this number has all bits set so I must mask off the unneeded bits for
        texture wraparound.

    SpanPtr is a pointer to the SpanArray buffer, this is where I store the
    processed floor textures.
    No light shading is used. The scale factor is a constant.
    
**********************************/

void DrawFloorColumn(Word ds_y,Word ds_x1,Word Count,LongWord xfrac,
    LongWord yfrac,Fixed ds_xstep,Fixed ds_ystep)
{
    // TODO: TEMP - CLEANUP
    const uint16_t* const pPLUT = (const uint16_t*) PlaneSource;
    const Fixed lightMultiplier = getLightMultiplier(tx_texturelight, MAX_FLOOR_LIGHT_VALUE);

    for (uint32_t pixelNum = 0; pixelNum < Count; ++pixelNum) {
        Fixed tx = ((xfrac + ds_xstep * pixelNum) >> FRACBITS) & 63;    // assumes 64x64
        Fixed ty = ((yfrac + ds_ystep * pixelNum) >> FRACBITS) & 63;    // assumes 64x64

        Fixed offset = ty * 64 + tx;
        const Byte lutByte = PlaneSource[64 + offset] & 31;
        ASSERT(lutByte < 32);
        uint8_t colorIdx = lutByte;

        const uint16_t color = byteSwappedU16(pPLUT[colorIdx]);
        const uint16_t texR = (color & 0b0111110000000000) >> 10;
        const uint16_t texG = (color & 0b0000001111100000) >> 5;
        const uint16_t texB = (color & 0b0000000000011111) >> 0;

        const Fixed texRFrac = int32ToSFixed16_16(texR);
        const Fixed texGFrac = int32ToSFixed16_16(texG);
        const Fixed texBFrac = int32ToSFixed16_16(texB);

        const Fixed darkenedR = sfixedMul16_16(texRFrac, lightMultiplier);
        const Fixed darkenedG = sfixedMul16_16(texGFrac, lightMultiplier);
        const Fixed darkenedB = sfixedMul16_16(texBFrac, lightMultiplier);

        const uint32_t finalColor = makeFramebufferColor(darkenedR, darkenedG, darkenedB);        
        const uint32_t screenX = ds_x1 + pixelNum + ScreenXOffset;
        const uint32_t screenY = ds_y + ScreenYOffset;

        Video::gFrameBuffer[screenY * Video::SCREEN_WIDTH + screenX] = finalColor;
    }

    // DC: FIXME: implement/replace
    #if 0
        Byte *DestPtr;
        MyCCB *DestCCB;

        DestCCB = CurrentCCB;       /* Copy pointer to local */
        if (DestCCB>=&CCBArray[CCBTotal]) {     /* Am I full already? */
            FlushCCBs();                /* Draw all the CCBs/Lines */
            DestCCB=CCBArray;
        }
        DestPtr = SpanPtr;
        DrawASpan(Count,xfrac,yfrac,ds_xstep,ds_ystep,DestPtr);

        DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT|CCB_USEAV;  /* ccb_flags */

        DestCCB->ccb_PRE0 = 0x00000005;     /* Preamble (Coded 8 bit) */
        DestCCB->ccb_PRE1 = 0x3E005000|(Count-1);       /* Second preamble */
        DestCCB->ccb_SourcePtr = (CelData *)DestPtr;    /* Save the source ptr */
        DestCCB->ccb_PLUTPtr = PlaneSource;     /* Get the palette ptr */
        DestCCB->ccb_XPos = ds_x1<<16;      /* Set the x and y coord for start */
        DestCCB->ccb_YPos = ds_y<<16;
        DestCCB->ccb_HDX = 1<<20;       /* OK */
        DestCCB->ccb_HDY = 0<<20;
        DestCCB->ccb_VDX = 0<<16;
        DestCCB->ccb_VDY = 1<<16;
        DestCCB->ccb_PIXC = LightTable[tx_texturelight>>LIGHTSCALESHIFT];           /* PIXC control */
    
        Count = (Count+3)&(~3);     /* Round to nearest longword */
        DestPtr += Count;
        SpanPtr = DestPtr;
        ++DestCCB;              /* Next CCB */
        CurrentCCB = DestCCB;   /* Save the CCB pointer */
    #endif
}


/**********************************

    Perform a "Doom" like screen wipe
    I require that VideoPointer is set to the current screen

**********************************/

#define WIPEWIDTH 320       /* Width of the 3DO screen to wipe */
#define WIPEHEIGHT 200

// DC: TODO: unused currently
#if 0
void WipeDoom(LongWord *OldScreen,LongWord *NewScreen)
{
    LongWord Mark;  /* Last time mark */
    Word TimeCount; /* Elapsed time since last mark */
    Word i,x;
    Word Quit;      /* Finish flag */
    int delta;      /* YDelta (Must be INT!) */
    LongWord *Screenad;     /* I use short pointers so I can */
    LongWord *SourcePtr;        /* move in pixel pairs... */
    int YDeltaTable[WIPEWIDTH/2];   /* Array of deltas for the jagged look */

/* First thing I do is to create a ydelta table to */
/* allow the jagged look to the screen wipe */

    delta = -GetRandom(15); /* Get the initial position */
    YDeltaTable[0] = delta; /* Save it */
    x = 1;
    do {
        delta += (GetRandom(2)-1);  /* Add -1,0 or 1 */
        if (delta>0) {      /* Too high? */
            delta = 0;
        }
        if (delta == -16) { /* Too low? */
            delta = -15;
        }
        YDeltaTable[x] = delta; /* Save the delta in table */
    } while (++x<(WIPEWIDTH/2));    /* Quit? */

/* Now perform the wipe using ReadTick to generate a time base */
/* Do NOT go faster than 30 frames per second */

    Mark = ReadTick()-2;    /* Get the initial time mark */
    do {
        do {
            TimeCount = ReadTick()-Mark;    /* Get the time mark */
        } while (TimeCount<(TICKSPERSEC/30));           /* Enough time yet? */
        Mark+=TimeCount;        /* Adjust the base mark */
        TimeCount/=(TICKSPERSEC/30);    /* Math is for 30 frames per second */

/* Adjust the YDeltaTable "TimeCount" times to adjust for slow machines */

        Quit = true;        /* Assume I already am finished */
        do {
            x = 0;      /* Start at the left */
            do {
                delta = YDeltaTable[x];     /* Get the delta */
                if (delta<WIPEHEIGHT) { /* Line finished? */
                    Quit = false;       /* I changed one! */
                    if (delta < 0) {
                        ++delta;        /* Slight delay */
                    } else if (delta < 16) {
                        delta = delta<<1;   /* Double it */
                        ++delta;
                    } else {
                        delta+=8;       /* Constant speed */
                        if (delta>WIPEHEIGHT) {
                            delta=WIPEHEIGHT;
                        }
                    }
                    YDeltaTable[x] = delta; /* Save new delta */
                }
            } while (++x<(WIPEWIDTH/2));
        } while (--TimeCount);      /* All tics accounted for? */

/* Draw a frame of the wipe */

        x = 0;          /* Init the x coord */
        do {
            Screenad = (LongWord *)&VideoPointer[x*8];  /* Dest pointer */
            i = YDeltaTable[x];     /* Get offset */
            if ((int)i<0) { /* Less than zero? */
                i = 0;      /* Make it zero */
            }
            i>>=1;      /* Force even for 3DO weirdness */
            if (i) {
                TimeCount = i;
                SourcePtr = &NewScreen[x*2];    /* Fetch from source */
                do {
                    Screenad[0] = SourcePtr[0]; /* Copy 2 longwords */
                    Screenad[1] = SourcePtr[1];
                    Screenad+=WIPEWIDTH;
                    SourcePtr+=WIPEWIDTH;
                } while (--TimeCount);
            }
            if (i<(WIPEHEIGHT/2)) {     /* Any of the old image to draw? */
                i = (WIPEHEIGHT/2)-i;
                SourcePtr = &OldScreen[x*2];
                do {
                    Screenad[0] = SourcePtr[0]; /* Copy 2 longwords */
                    Screenad[1] = SourcePtr[1];
                    Screenad+=WIPEWIDTH;
                    SourcePtr+=WIPEWIDTH;
                } while (--i);
            }
        } while (++x<(WIPEWIDTH/2));
    } while (!Quit);        /* Wipe finished? */
}
#endif

static Byte *SpritePLUT;
static Word SpriteY;
static Word SpriteYScale;
static Word SpritePIXC;
static Word SpritePRE0;
static Word SpritePRE1;
static Byte *StartLinePtr;
static Word SpriteWidth;

static Byte* CalcLine(Fixed XFrac)
{
    Byte *DataPtr;
    
    DataPtr = StartLinePtr;
    XFrac>>=FRACBITS;
    if (XFrac<=0) {     /* Left clip failsafe */
        return DataPtr;
    }
    if (XFrac>= (int) SpriteWidth) {   /* Clipping failsafe */
        XFrac=(int) SpriteWidth-1;
    }
    do {
        Word Offset;
        Offset = DataPtr[0]+2;
        DataPtr = &DataPtr[Offset*4];
    } while (--XFrac);
    return DataPtr;
}

//-------------------------------------------------------------------------------------------------
// Get the pointer to the first pixel in a particular column of a sprite.
// The input x coordinate is in 16.16 fixed point format and the output is clamped to sprite bounds.
//-------------------------------------------------------------------------------------------------
static const uint16_t* getSpriteColumn(const vissprite_t* const pVisSprite, const Fixed xFrac) {
    const int32_t x = sfixed16_16ToInt32(xFrac);
    const SpriteFrameAngle* const pSprite = pVisSprite->pSprite;
    const int32_t xClamped = (x < 0) ? 0 : ((x >= pSprite->width) ? pSprite->width - 1 : x);
    return &pSprite->pTexture[pSprite->height * xClamped];
}

//-------------------------------------------------------------------------------------------------
// Utility that determines how much to step (in texels) per pixel to render the entire of the given
// sprite dimension in the given render area dimension (both in pixels).
//-------------------------------------------------------------------------------------------------
static Fixed determineTexelStep(const uint32_t textureSize, const uint32_t renderSize) {
    if (textureSize <= 1 || renderSize <= 1) {
        return 0;
    }
    
    // The way the math works here helps ensure the last pixel drawn is pretty much always the last pixel
    // of the texture. This ensures that edges/borders around sprites etc. don't seem to vanish... 
    // I used to have issues with the bottom rows of the explosive barrels cutting out before adopting this method.
    const int32_t numPixelSteps = (int32_t) renderSize - 1;
    const Fixed step = sfixedDiv16_16(
        int32ToSFixed16_16(textureSize) - 1,    // N.B: never let it reach 'textureSize' - keep below, as that is an out of bounds index!
        int32ToSFixed16_16(numPixelSteps)
    );

    return step;
}

//-------------------------------------------------------------------------------------------------
// This routine will draw a scaled sprite during the game. It is called when there is no 
// onscreen clipping needed or if the only clipping is to the screen bounds.
//-------------------------------------------------------------------------------------------------
void DrawSpriteNoClip(const vissprite_t* const pVisSprite) {
    // Get the left, right, top and bottom screen edges for the sprite to be rendered.
    // Also check if the sprite is completely offscreen, because the input is not clipped.
    // 3DO Doom originally relied on the hardware to clip in this routine...
    int x1 = pVisSprite->x1;
    int x2 = pVisSprite->x2;
    int y1 = pVisSprite->y1;
    int y2 = pVisSprite->y2;

    const bool bCompletelyOffscreen = (
        (x1 >= (int) ScreenWidth) ||
        (x2 < 0) ||
        (y1 >= (int) ScreenHeight) ||
        (y2 < 0)
    );

    if (bCompletelyOffscreen)
        return;

    // Get the light multiplier to use for lighting the sprite
    const Fixed lightMultiplier = getLightMultiplier(pVisSprite->colormap & 0xFF, MAX_SPRITE_LIGHT_VALUE);
    
    // Get the width and height of the sprite and also the size that it will be rendered at.
    // Note that we expect no zero sizes here!
    const SpriteFrameAngle* const pSpriteFrame = pVisSprite->pSprite;
    
    const int32_t spriteW = pSpriteFrame->width;
    const int32_t spriteH = pSpriteFrame->height;
    ASSERT(spriteW > 0 && spriteH > 0);

    const uint32_t renderW = (pVisSprite->x2 - pVisSprite->x1) + 1;
    const uint32_t renderH = (pVisSprite->y2 - pVisSprite->y1) + 1;
    ASSERT(renderW > 0 && renderH > 0);

    // Figure out the step in texels we want per x and y pixel in 16.16 format
    const Fixed texelStepX_NoFlip = determineTexelStep(spriteW, renderW);
    const Fixed texelStepY = determineTexelStep(spriteH, renderH);

    // Computing start texel x coord (y is '0' for now) and step due to sprite flipping
    Fixed texelStepX;
    Fixed startTexelX;
    Fixed startTexelY = 0;

    if (pSpriteFrame->flipped != 0) {
        texelStepX = -texelStepX_NoFlip;
        startTexelX = int32ToSFixed16_16(spriteW) - 1;    // Start from the furthest reaches of the end pixel, only want texel to change if nearly 1 unit has been passed!
    }
    else {
        texelStepX = texelStepX_NoFlip;
        startTexelX = 0;
    }

    // Sanity check in debug that we won't go out of bounds of the texture (shouldn't)
    #if ASSERTS_ENABLED == 1
    {
        const Fixed endXFrac = startTexelX + sfixedMul16_16(texelStepX, int32ToSFixed16_16(renderW - 1));
        const Fixed endYFrac = sfixedMul16_16(texelStepY, int32ToSFixed16_16(renderH - 1));
        const int32_t endX = sfixed16_16ToInt32(endXFrac);
        const int32_t endY = sfixed16_16ToInt32(endYFrac);

        ASSERT(endX >= 0 && endX < spriteW);
        ASSERT(endY >= 0 && endY < spriteH);
    }
    #endif

    // Clip the sprite render bounds to the screen (left, right, top, bottom, in that order).
    // Skip over rows and columns that are out of bounds:
    if (x1 < 0) {
        const int pixelsOffscreen = -x1;
        startTexelX += pixelsOffscreen * texelStepX;
        x1 = 0;
    }

    if (x2 >= (int) ScreenWidth) {
        x2 = ScreenWidth - 1;
    }
    
    if (y1 < 0) {
        const int pixelsOffscreen = -y1;
        startTexelY += pixelsOffscreen * texelStepY;
        y1 = 0;
    }

    if (y2 >= (int) ScreenHeight) {
        y2 = ScreenHeight - 1;
    }
    
    // Render all the columns of the sprite
    const uint16_t* const pImage = pSpriteFrame->pTexture;
    Fixed texelXFrac = startTexelX;

    for (int x = x1; x <= x2; ++x) {
        const int texelXInt = sfixed16_16ToInt32(texelXFrac);
        Fixed texelYFrac = startTexelY;
        
        const uint16_t* const pImageCol = pImage + texelXInt * spriteH;
        uint32_t* pDstPixel = &Video::gFrameBuffer[x + ScreenXOffset + (y1 + ScreenYOffset) * Video::SCREEN_WIDTH];

        for (int y = y1; y <= y2; ++y) {
            // Grab this pixels color from the sprite image and skip if alpha 0
            const int texelYInt = sfixed16_16ToInt32(texelYFrac);

            const uint16_t color = pImageCol[texelYInt];
            const uint16_t texA = (color & 0b1000000000000000) >> 15;
            const uint16_t texR = (color & 0b0111110000000000) >> 10;
            const uint16_t texG = (color & 0b0000001111100000) >> 5;
            const uint16_t texB = (color & 0b0000000000011111) >> 0;

            if (texA != 0) {
                // Do light diminishing
                const Fixed texRFrac = int32ToSFixed16_16(texR);
                const Fixed texGFrac = int32ToSFixed16_16(texG);
                const Fixed texBFrac = int32ToSFixed16_16(texB);

                const Fixed darkenedR = sfixedMul16_16(texRFrac, lightMultiplier);
                const Fixed darkenedG = sfixedMul16_16(texGFrac, lightMultiplier);
                const Fixed darkenedB = sfixedMul16_16(texBFrac, lightMultiplier);

                // Save the final output color
                const uint32_t finalColor = makeFramebufferColor(darkenedR, darkenedG, darkenedB);
                *pDstPixel = finalColor;
            }

            // Onto the next pixel in the column
            texelYFrac += texelStepY;
            pDstPixel += Video::SCREEN_WIDTH;
        }

        texelXFrac += texelStepX;   // Next column
    }

    /*
    patch_t *patch;     // Pointer to the actual sprite record
    Word ColorMap;
    int x;
    
    patch = (patch_t *)loadResourceData(vis->PatchLump);   
    patch =(patch_t *) &((Byte *)patch)[vis->PatchOffset];

    ((LongWord *)patch)[7] = 0;
    ((LongWord *)patch)[10] = 0;
    ((LongWord *)patch)[8] = vis->yscale<<4;
    ColorMap = vis->colormap;
    if (ColorMap&0x8000) {
        ((LongWord *)patch)[13] = 0x9C81;
    } else {
        ((LongWord *)patch)[13] = LightTable[(ColorMap&0xFF)>>LIGHTSCALESHIFT];
    }
    if (ColorMap&0x4000) {
        x = vis->x2;
        ((LongWord *)patch)[9] = -vis->xscale;
    } else {
        x = vis->x1;
        ((LongWord *)patch)[9] = vis->xscale;
    }
    DrawMShape(x+ScreenXOffset,vis->y1+ScreenYOffset,&patch->Data);
    releaseResource(vis->PatchLump);
    */
}

//-------------------------------------------------------------------------------------------------
// Draws a column of a sprite that is clipped to the given top and bottom bounds
//-------------------------------------------------------------------------------------------------
static void OneSpriteLine(
    const uint32_t screenX,
    const Fixed spriteX,
    const uint32_t topClipY,
    const uint32_t bottomClipY,
    const vissprite_t* const pVisSprite
)
{
    // Sanity checks
    ASSERT(topClipY >= 0 && topClipY <= ScreenHeight);
    ASSERT(bottomClipY >= 0 && bottomClipY <= ScreenHeight);
    ASSERT(pVisSprite);

    // Get the top and bottom screen edges for the sprite columnn to be rendered.
    // Also check if the column is completely offscreen, because the input is not clipped.
    // 3DO Doom originally relied on the hardware to clip in this routine...
    if (screenX < 0 || screenX >= (int) ScreenWidth)
        return;
    
    int y1 = pVisSprite->y1;
    int y2 = pVisSprite->y2;
    
    if (y1 >= (int) ScreenHeight || y2 < 0)
        return;
    
    // If the clip bounds are meeting (or past each other?!) then ignore
    if (topClipY >= bottomClipY)
        return;
    
    // Get the light multiplier to use for lighting the sprite
    const Fixed lightMultiplier = getLightMultiplier(pVisSprite->colormap & 0xFF, MAX_SPRITE_LIGHT_VALUE);
    
    // Get the height of the sprite and also the height that it will be rendered at.
    // Note that we expect no zero sizes here!
    const SpriteFrameAngle* const pSpriteFrame = pVisSprite->pSprite;
    
    const int32_t spriteH = pSpriteFrame->height;
    const uint32_t renderH = (pVisSprite->y2 - pVisSprite->y1) + 1;
    ASSERT(spriteH > 0);
    ASSERT(renderH > 0);
    
    // Figure out the step in texels we want per y pixel in 16.16 format
    const Fixed texelStepY = determineTexelStep(spriteH, renderH);

    // Sanity check in debug that we won't go out of bounds of the texture (shouldn't)
    #if ASSERTS_ENABLED == 1
    {
        const Fixed endYFrac = sfixedMul16_16(texelStepY, int32ToSFixed16_16(renderH - 1));
        const int32_t endY = sfixed16_16ToInt32(endYFrac);
        ASSERT(endY >= 0 && endY < spriteH);
    }
    #endif
    
    // Clip the sprite render bounds to the screen (top, bottom, in that order).
    // Skip over rows that are out of bounds:
    Fixed startTexelY = 0;
    
    if (y1 < (int) topClipY) {
        const int pixelsOffscreen = topClipY - y1;
        startTexelY += pixelsOffscreen * texelStepY;
        y1 = topClipY;
    }
    
    if (y2 >= (int) bottomClipY) {
        y2 = bottomClipY - 1;
    }
    
    // Render the sprite column
    const uint16_t* const pImageCol = getSpriteColumn(pVisSprite, spriteX);
    Fixed texelYFrac = startTexelY;
    uint32_t* pDstPixel = &Video::gFrameBuffer[screenX + ScreenXOffset + (y1 + ScreenYOffset) * Video::SCREEN_WIDTH];

    for (int y = y1; y <= y2; ++y) {
        // Grab this pixels color from the sprite image and skip if alpha 0
        const int texelYInt = sfixed16_16ToInt32(texelYFrac);
        
        const uint16_t color = pImageCol[texelYInt];
        const uint16_t texA = (color & 0b1000000000000000) >> 15;
        const uint16_t texR = (color & 0b0111110000000000) >> 10;
        const uint16_t texG = (color & 0b0000001111100000) >> 5;
        const uint16_t texB = (color & 0b0000000000011111) >> 0;

        if (texA != 0) {
            // Do light diminishing
            const Fixed texRFrac = int32ToSFixed16_16(texR);
            const Fixed texGFrac = int32ToSFixed16_16(texG);
            const Fixed texBFrac = int32ToSFixed16_16(texB);

            const Fixed darkenedR = sfixedMul16_16(texRFrac, lightMultiplier);
            const Fixed darkenedG = sfixedMul16_16(texGFrac, lightMultiplier);
            const Fixed darkenedB = sfixedMul16_16(texBFrac, lightMultiplier);
            
            // Save the final output color
            const uint32_t finalColor = makeFramebufferColor(darkenedR, darkenedG, darkenedB);
            *pDstPixel = finalColor;
        }
        
        // Onto the next pixel in the column
        texelYFrac += texelStepY;
        pDstPixel += Video::SCREEN_WIDTH;
    }

    // DC: FIXME: implement/replace
    #if 0
        MyCCB *DestCCB;

        DestCCB = CurrentCCB;       /* Copy pointer to local */
        if (DestCCB>=&CCBArray[CCBTotal]) {     /* Am I full already? */
            FlushCCBs();                /* Draw all the CCBs/Lines */
            DestCCB=CCBArray;
        }
        DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_PACKED|
        CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
        CCB_ACE|CCB_BGND|CCB_NOBLK|CCB_PPABS|CCB_LDPLUT;    /* ccb_flags */

        DestCCB->ccb_PIXC = SpritePIXC;         /* PIXC control */
        DestCCB->ccb_PRE0 = SpritePRE0;     /* Preamble (Coded 8 bit) */
        DestCCB->ccb_PRE1 = SpritePRE1;     /* Second preamble */
        DestCCB->ccb_SourcePtr = (CelData *)SpriteLinePtr;  /* Save the source ptr */
        DestCCB->ccb_PLUTPtr = SpritePLUT;      /* Get the palette ptr */
        DestCCB->ccb_XPos = (x1+ScreenXOffset)<<16;     /* Set the x and y coord for start */
        DestCCB->ccb_YPos = SpriteY;
        DestCCB->ccb_HDX = 0<<20;       /* OK */
        DestCCB->ccb_HDY = SpriteYScale;
        DestCCB->ccb_VDX = 1<<16;
        DestCCB->ccb_VDY = 0<<16;
        ++DestCCB;          /* Next CCB */
        CurrentCCB = DestCCB;   /* Save the CCB pointer */
    #endif
}


// DC: FIXME: implement/replace
#if 0
/**********************************

    This routine will draw a scaled sprite during the game.
    It is called when there is onscreen clipping needed so I
    use the global table spropening to get the top and bottom clip
    bounds.
 
    I am passed the screen clipped x1 and x2 coords.

**********************************/
static void OneSpriteClipLine(Word x1,Byte *SpriteLinePtr,int Clip,int Run)
{
    MyCCB *DestCCB;

    DrawARect(0,191,Run,1,BLACK);
    DestCCB = CurrentCCB;       /* Copy pointer to local */
    if (DestCCB>=&CCBArray[CCBTotal-1]) {       /* Am I full already? */
        FlushCCBs();                /* Draw all the CCBs/Lines */
        DestCCB=CCBArray;
    }
    DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|CCB_PACKED|
    CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
    CCB_ACE|CCB_BGND|CCB_PPABS|CCB_LDPLUT;  /* ccb_flags */

    DestCCB->ccb_PIXC = 0x1F00;         /* PIXC control */
    DestCCB->ccb_PRE0 = SpritePRE0;     /* Preamble (Coded 8 bit) */
    DestCCB->ccb_PRE1 = SpritePRE1;     /* Second preamble */
    DestCCB->ccb_SourcePtr = (CelData *)SpriteLinePtr;  /* Save the source ptr */
    DestCCB->ccb_PLUTPtr = SpritePLUT;      /* Get the palette ptr */
    DestCCB->ccb_XPos = -(Clip<<16);        /* Set the x and y coord for start */
    DestCCB->ccb_YPos = 191<<16;
    DestCCB->ccb_HDX = SpriteYScale;        /* OK */
    DestCCB->ccb_HDY = 0<<20;
    DestCCB->ccb_VDX = 0<<16;
    DestCCB->ccb_VDY = 1<<16;
    ++DestCCB;          /* Next CCB */

    DestCCB->ccb_Flags = CCB_SPABS|CCB_LDSIZE|CCB_LDPRS|
    CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
    CCB_ACE|CCB_NOBLK|CCB_PPABS;    /* ccb_flags */

    DestCCB->ccb_PIXC = SpritePIXC;         /* PIXC control */
    DestCCB->ccb_PRE0 = 0x00000016;     /* Preamble (Uncoded 16) */
    DestCCB->ccb_PRE1 = 0x9E001800+(Run-1);     /* Second preamble */
    DestCCB->ccb_SourcePtr = (CelData *)CelLine190; /* Save the source ptr */
    DestCCB->ccb_XPos = (x1+ScreenXOffset)<<16;     /* Set the x and y coord for start */
    DestCCB->ccb_YPos = SpriteY+(Clip<<16);
    DestCCB->ccb_HDX = 0<<20;       /* OK */
    DestCCB->ccb_HDY = 1<<20;
    DestCCB->ccb_VDX = 1<<15;       /* Need 15 to fix the LFORM bug */
    DestCCB->ccb_VDY = 0<<16;
    ++DestCCB;          /* Next CCB */

    CurrentCCB = DestCCB;   /* Save the CCB pointer */
}
#endif

//-------------------------------------------------------------------------------------------------
// Draws a clipped sprite to the screen
//-------------------------------------------------------------------------------------------------
void DrawSpriteClip(const Word x1, const Word x2, const vissprite_t* const pVisSprite) {
    SpriteYScale = pVisSprite->yscale << 4;                 // Get scale Y factor
    StartLinePtr = (Byte*) pVisSprite->pSprite->pTexture;   // Get pointer to first line of data
    SpriteWidth = pVisSprite->pSprite->width;
    SpritePIXC = (pVisSprite->colormap & 0x8000) ? 0x9C81 : LightTable[(pVisSprite->colormap & 0xFF) >> LIGHTSCALESHIFT];
    Word y = pVisSprite->y1;
    SpriteY = (y + ScreenYOffset) << 16;    // Unmolested Y coord
    Word y2 = pVisSprite->y2;
    Word MaxRun = y2 - y;
    
    if ((int) y < 0) {
        y = 0;
    }
    
    if ((int) y2 >= (int) ScreenHeight) {
        y2 = ScreenHeight;
    }
    
    Fixed XFrac = 0;
    Fixed XStep = 0xFFFFFFFFUL / (LongWord) pVisSprite->xscale;   // Get the recipocal for the X scale
    
    if (pVisSprite->colormap & 0x4000) {
        XStep = -XStep; // Step in the opposite direction
        XFrac = (SpriteWidth << FRACBITS) - 1;
    }
    
    if (pVisSprite->x1 != x1) { // How far should I skip?
        XFrac += XStep * (x1 - pVisSprite->x1);
    }
    
    uint32_t x = x1;
    
    do {
        Word top = spropening[x];   // Get the opening to the screen
        
        if (top == ScreenHeight) {  // Not clipped?
            OneSpriteLine(x, XFrac, 0, ScreenHeight, pVisSprite);
        } else {
            Word bottom = top & 0xff;
            top >>= 8;
            
            if (top < bottom) { // Valid run?
                if (y >= top && y2 < bottom) {
                    OneSpriteLine(x, XFrac, 0, ScreenHeight, pVisSprite);
                } else {
                    int Clip = top - pVisSprite->y1;    // Number of pixels to clip
                    int Run = bottom - top;             // Allowable run
                    
                    if (Clip < 0) {     // Overrun?
                        Run += Clip;    // Remove from run
                        Clip = 0;
                    }
                    
                    if (Run > 0) {              // Still visible?
                        if (Run > MaxRun) {     // Too big?
                            Run = MaxRun;       // Force largest...
                        }
                        
                        OneSpriteLine(x, XFrac, top, bottom, pVisSprite);
                    }
                }
            }
        }
        XFrac += XStep;
    } while (++x <= x2);

    #if 0
    Word y,MaxRun;
    Word y2;
    Word top,bottom;
    patch_t *patch;
    Fixed XStep,XFrac;
    
    patch = (patch_t *)loadResourceData(vis->PatchLump);   /* Get shape data */
    patch =(patch_t *) &((Byte *)patch)[vis->PatchOffset];  /* Get true pointer */
    SpriteYScale = vis->yscale<<4;      /* Get scale Y factor */
    SpritePLUT = &((Byte *)patch)[64];  /* Get pointer to PLUT */
    SpritePRE0 = ((Word *)patch)[14]&~(0xFF<<6);    /* Only 1 row allowed! */
    SpritePRE1 = ((Word *)patch)[15];       /* Get the proper height */
    y = ((Word *)patch)[3];     /* Get offset to the sprite shape data */
    StartLinePtr = &((Byte *)patch)[y+16];  /* Get pointer to first line of data */
    SpriteWidth = getCCBHeight(&((Word *)patch)[1]);
    SpritePIXC = (vis->colormap&0x8000) ? 0x9C81 : LightTable[(vis->colormap&0xFF)>>LIGHTSCALESHIFT];
    y = vis->y1;
    SpriteY = (y+ScreenYOffset)<<16;    /* Unmolested Y coord */
    y2 = vis->y2;
    MaxRun = y2-y;
    
    if ((int)y<0) {
        y = 0;
    }
    if ((int)y2>=(int)ScreenHeight) {
        y2 = ScreenHeight;
    }
    XFrac = 0;
    XStep = 0xFFFFFFFFUL/(LongWord)vis->xscale; /* Get the recipocal for the X scale */
    if (vis->colormap&0x4000) {
        XStep = -XStep;     /* Step in the opposite direction */
        XFrac = (SpriteWidth<<FRACBITS)-1;
    }
    if (vis->x1!=x1) {      /* How far should I skip? */
        XFrac += XStep*(x1-vis->x1);
    }
    do {
        top = spropening[x1];       /* Get the opening to the screen */
        if (top==ScreenHeight) {        /* Not clipped? */
            OneSpriteLine(x1,CalcLine(XFrac));
        } else {
            bottom = top&0xff;
            top >>=8;
            if (top<bottom) {       /* Valid run? */
                if (y>=top && y2<bottom) {
                    OneSpriteLine(x1,CalcLine(XFrac));
                } else {
                    int Run;
                    int Clip;
                    
                    Clip = top-vis->y1;     /* Number of pixels to clip */
                    Run = bottom-top;       /* Allowable run */
                    if (Clip<0) {       /* Overrun? */
                        Run += Clip;    /* Remove from run */
                        Clip = 0;
                    }
                    if (Run>0) {        /* Still visible? */
                        if (Run>MaxRun) {       /* Too big? */
                            Run = MaxRun;       /* Force largest... */
                        }
                        OneSpriteClipLine(x1,CalcLine(XFrac),Clip,Run);
                    }
                }
            }
        }
        XFrac+=XStep;
    } while (++x1<=x2);
    #endif
}

/**********************************

    Draw a sprite in the center of the screen.
    This is used for the finale.
    (Speed is NOT an issue here...)

**********************************/

void DrawSpriteCenter(Word SpriteNum)
{
    // DC: FIXME: implement/replace
    #if 0
        Word x,y;
        patch_t *patch;
        LongWord Offset;

        patch = (patch_t *)LoadAResource(SpriteNum>>FF_SPRITESHIFT);    /* Get the sprite group */
        Offset = ((LongWord *)patch)[SpriteNum & FF_FRAMEMASK];
        if (Offset&PT_NOROTATE) {       /* Do I rotate? */
            patch = (patch_t *) &((Byte *)patch)[Offset & 0x3FFFFFFF];      /* Get pointer to rotation list */
            Offset = ((LongWord *)patch)[0];        /* Use the rotated offset */
        }
        patch = (patch_t *)&((Byte *)patch)[Offset & 0x3FFFFFFF];   /* Get pointer to patch */
    
        x = patch->leftoffset;      /* Get the x and y offsets */
        y = patch->topoffset;
        x = 80-x;           /* Center on the screen */
        y = 90-y;
        ((LongWord *)patch)[7] = 0;     /* Compensate for sideways scaling */
        ((LongWord *)patch)[10] = 0;
        if (Offset&PT_FLIP) {
            ((LongWord *)patch)[8] = -0x2<<20;  /* Reverse horizontal */
            x+=GetShapeHeight(&patch->Data);    /* Adjust the x coord */
        } else {
            ((LongWord *)patch)[8] = 0x2<<20;   /* Normal horizontal */
        }
        ((LongWord *)patch)[9] = 0x2<<16;       /* Double vertical */
        DrawMShape(x*2,y*2,&patch->Data);       /* Scale the x and y and draw */
        ReleaseAResource(SpriteNum>>FF_SPRITESHIFT);    /* Let go of the resource */
    #endif
}

/**********************************

    Set the hardware clip rect to the actual game screen
    
**********************************/

void EnableHardwareClipping(void)
{
    // DC: FIXME: implement/replace
    #if 0
        FlushCCBs();                                            /* Failsafe */
        SetClipWidth(VideoItem,ScreenWidth);
        SetClipHeight(VideoItem,ScreenHeight);                  /* I only want 200 lines */
        SetClipOrigin(VideoItem,ScreenXOffset,ScreenYOffset);   /* Set the clip top for the screen */
    #endif
}

/**********************************

    Restore the clip rect to normal
    
**********************************/

void DisableHardwareClipping(void)
{
    // DC: FIXME: implement/replace
    #if 0
        FlushCCBs();                        /* Failsafe */
        SetClipOrigin(VideoItem,0,0);       /* Set the clip top for the screen */
        SetClipWidth(VideoItem,320);
        SetClipHeight(VideoItem,200);       /* I only want 200 lines */
    #endif
}

/**********************************

    Draw a screen color overlay if needed
    
**********************************/

void DrawColors(void)
{
    // DC: FIXME: implement/replace
    #if 0
        MyCCB* DestCCB;         /* Pointer to new CCB entry */
        player_t *player;
        Word ccb,color;
        Word red,green,blue;
    
        player = &players;
        if (player->powers[pw_invulnerability] > 240        /* Full strength */
            || (player->powers[pw_invulnerability]&16) ) {  /* Flashing... */
            color = 0x7FFF<<16;
            ccb = CCB_LDSIZE|CCB_LDPRS|CCB_PXOR|
            CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
            CCB_ACE|CCB_BGND|CCB_NOBLK;
            goto DrawIt;
        }
    
        red = player->damagecount;      /* Get damage inflicted */
        green = player->bonuscount>>2;
        red += green;
        blue = 0;
    
        if (player->powers[pw_ironfeet] > 240       /* Radiation suit? */
            || (player->powers[pw_ironfeet]&16) ) { /* Flashing... */
            green = 10;         /* Add some green */
        }

        if (player->powers[pw_strength]         /* Berserker pack? */
            && (player->powers[pw_strength]< 255) ) {
            color = 255-player->powers[pw_strength];
            color >>= 4;
            red+=color;     /* Feeling good! */
        }

        if (red>=32) {
            red = 31;
        }
        if (green>=32) {
            green =31;
        }
        if (blue>=32) {
            blue = 31;
        }

        color = (red<<10)|(green<<5)|blue;
    
        if (!color) {
            return;
        }
        color <<=16;
        ccb = CCB_LDSIZE|CCB_LDPRS|
            CCB_LDPPMP|CCB_CCBPRE|CCB_YOXY|CCB_ACW|CCB_ACCW|
            CCB_ACE|CCB_BGND|CCB_NOBLK;

    DrawIt:
        DestCCB = CurrentCCB;       /* Copy pointer to local */
        if (DestCCB>=&CCBArray[CCBTotal]) {     /* Am I full already? */
            FlushCCBs();                /* Draw all the CCBs/Lines */
            DestCCB=CCBArray;
        }
    
        DestCCB->ccb_Flags =ccb;    /* ccb_flags */
        DestCCB->ccb_PIXC = 0x1F80;     /* PIXC control */
        DestCCB->ccb_PRE0 = 0x40000016;     /* Preamble */
        DestCCB->ccb_PRE1 = 0x03FF1000;     /* Second preamble */
        DestCCB->ccb_SourcePtr = (CelData *)0;  /* Save the source ptr */
        DestCCB->ccb_PLUTPtr = (void *)color;       /* Set the color pixel */
        DestCCB->ccb_XPos = ScreenXOffset<<16;      /* Set the x and y coord for start */
        DestCCB->ccb_YPos = ScreenYOffset<<16;
        DestCCB->ccb_HDX = ScreenWidth<<20;     /* OK */
        DestCCB->ccb_HDY = 0<<20;
        DestCCB->ccb_VDX = 0<<16;
        DestCCB->ccb_VDY = ScreenHeight<<16;
        ++DestCCB;          /* Next CCB */
        CurrentCCB = DestCCB;   /* Save the CCB pointer */
    #endif
}

#if 0
/**********************************

    This will allow screen shots to be taken.
    REMOVE FOR FINAL BUILD!!!

**********************************/

Word LastJoyButtons[4];     /* Save the previous joypad bits */
static Word FileNum;
static Short OneLine[640];

Word ReadJoyButtons(Word PadNum)
{
    char FileName[20];
    ControlPadEventData ControlRec;
    Short *OldImage;
    Short *DestImage;

    GetControlPad(PadNum+1,FALSE,&ControlRec);      /* Read joypad */
    if (PadNum<4) {
        if (((ControlRec.cped_ButtonBits^LastJoyButtons[PadNum]) &
            ControlRec.cped_ButtonBits)&PadC) {
            Word i,j,PrevPage;
            
            sprintf(FileName,"Art%d.RAW16",FileNum);
            ++FileNum;
            PrevPage = WorkPage-1;  /* Get the currently displayed page */
            if (PrevPage==-1) {     /* Wrapped? */
                PrevPage = SCREENS-1;
            }
            OldImage = (Short *) &ScreenMaps[PrevPage][0];  /* Get work buffer */
            i = 0;
            DestImage = OldImage;
            do {
                memcpy(OneLine,DestImage,320*2*2);
                j = 0;
                do {
                    DestImage[j] = OneLine[j*2];
                    DestImage[j+320] = OneLine[(j*2)+1];
                } while (++j<320);
                DestImage+=640;
            } while (++i<100);
            WriteMacFile(FileName,OldImage,320*200*2);
        }
        LastJoyButtons[PadNum] = (Word)ControlRec.cped_ButtonBits;
    }
    return (Word)ControlRec.cped_ButtonBits;        /* Return the data */
}
#endif
