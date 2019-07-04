#include "ThreeDO.h"

#include "Audio/Audio.h"
#include "Base/Endian.h"
#include "Base/Macros.h"
#include "Base/Mem.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomMain.h"
#include "Game/Resources.h"
#include "GFX/CelUtils.h"
#include "GFX/Renderer.h"
#include "GFX/Sprites.h"
#include "GFX/Textures.h"
#include "GFX/Video.h"
#include <algorithm>

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

typedef struct MyCCB {  // Clone of the CCB Block from the 3DO includes 
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
} MyCCB;    // I DON'T include width and height 

static void FlushCCBs(void);

// DC: TODO: unused currently
#if 0
static void WipeDoom(LongWord *OldScreen,LongWord *NewScreen);
#endif

#define CCBTotal 0x200

static MyCCB gCCBArray[CCBTotal];            // Array of CCB structs 

// DC: TODO: unused currently
#if 0
static MyCCB *CurrentCCB = &gCCBArray[0];    // Pointer to empty CCB 
static LongWord gLastTicCount;               // Time mark for page flipping 
#endif

uint32_t gLastTics;         // Time elapsed since last page flip 
uint32_t gWorkPage;         // Which frame is not being displayed 

// DC: TODO: unused currently
#if 0
static Byte *gCelLine190;
#endif

uint8_t gSpanArray[MAXSCREENWIDTH*MAXSCREENHEIGHT]; // Buffer for floor textures 
uint8_t* gSpanPtr = gSpanArray;      // Pointer to empty buffer 

#define SKYSCALE(x) (Fixed)(1048576.0*(x/160.0))

// DC: TODO: unused currently
#if 0
static Fixed gSkyScales[6] = {
    SKYSCALE(160.0),
    SKYSCALE(144.0),
    SKYSCALE(128.0),
    SKYSCALE(112.0),
    SKYSCALE(96.0),
    SKYSCALE(80.0)
};
#endif

#define SCREENS 3                   // Need to page flip 
uint32_t gMainTask;                  // My own task item 

// DC: TODO: unused currently
#if 0
static uint32_t gScreenPageCount;    // Number of screens 
static Item gScreenItems[SCREENS];   // Referances to the game screens 
static Item gVideoItems[SCREENS];
static long gScreenByteCount;        // How many bytes for each screen 
static Item gScreenGroupItem = 0;    // Main screen referance 
static Byte *gScreenMaps[SCREENS];   // Pointer to the bitmap screens 
static Item gVRAMIOReq;              // I/O Request for screen copy 
#endif

/**********************************

    Run an external program and wait for compleation

**********************************/

// DC: 3DO specific - disable
#if 0
static void RunAProgram(char *ProgramName)
{
    Item LogoItem;
    LogoItem=LoadProgram(ProgramName);  // Load and begin execution 
    do {
        Yield();                        // Yield all CPU time to the other program 
    } while (LookupItem(LogoItem));     // Wait until the program quits 
    DeleteItem(LogoItem);               // Dispose of the 3DO logo code 
}
#endif

/**********************************

    Init the 3DO variables to a specific screen

**********************************/

static void SetMyScreen(uint32_t Page)
{
    // DC: FIXME: implement/replace
    #if 0
        VideoItem = gVideoItems[Page];           // Get the bitmap item # 
        VideoScreen = gScreenItems[Page];
        VideoPointer = (Byte *) &gScreenMaps[Page][0];
        gCelLine190 = (Byte *) &VideoPointer[190*640];
    #endif
}

/**********************************

    Init the system tools

    Start up all the tools for the 3DO system
    Return TRUE if all systems are GO!

**********************************/

// DC: TODO: unused currently
#if 0
static Word gHeightArray[1] = { 200 };   // I want 200 lines for display memory 
#endif

// DC: 3DO specific - disable
#if 0
    static Word gMyCustomVDL[] = {
        VDL_RELSEL|         // Relative pointer to next VDL 
        (1<<VDL_LEN_SHIFT)|     // (DMA) 1 control words in this VDL entry 
        (20<<VDL_LINE_SHIFT),   // Scan lines to persist 
        0,              // Current video buffer 
        0,              // Previous video buffer 
        4*4,            // Pointer to next vdl 
        0xE0000000,     // Set the screen to BLACK 
        VDL_NOP,        // Filler to align to 16 bytes 
        VDL_NOP,
        VDL_NOP,

        VDL_RELSEL|
        VDL_ENVIDDMA|               // Enable video DMA 
        VDL_LDCUR|                  // Load current address 
        VDL_LDPREV|                 // Load previous address 
        ((32+2)<<VDL_LEN_SHIFT)|            // (DMA) 2 control words in this VDL entry 
        (198<<VDL_LINE_SHIFT),      // Scan lines to persist 
        0,              // Current video buffer 
        0,              // Previous video buffer 
        (32+4)*4,           // Pointer to next vdl 

        VDL_DISPCTRL|   // Video display control word 
        VDL_CLUTBYPASSEN|   // Allow fixed clut 
        VDL_WINBLSB_BLUE|   // Normal blue 
    #if 1
        VDL_WINVINTEN|      // Enable HV interpolation 
        VDL_WINHINTEN|
        VDL_VINTEN|
        VDL_HINTEN|
    #endif
        VDL_BLSB_BLUE|      // Normal 
        VDL_HSUB_FRAME,     // Normal 
        0x00000000,     // Default CLUT 
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
        0xE0000000,     // Set the screen to BLACK 
        VDL_NOP,                // Filler to align to 16 bytes 
        VDL_NOP,

        (1<<VDL_LEN_SHIFT)|         // (DMA) 1 control words in this VDL entry 
        (0<<VDL_LINE_SHIFT),        // Scan lines to persist (Forever) 
        0,              // Current video buffer 
        0,              // Previous video buffer 
        0,              // Pointer to next vdl (None) 
        0xE0000000,     // Set the screen to BLACK 
        VDL_NOP,                // Filler to align to 16 bytes 
        VDL_NOP,
        VDL_NOP
    };

    static TagArg gScreenTags[] =    {       // Change this to change the screen count! 
        CSG_TAG_SPORTBITS, (void *)0,   // Allow SPORT DMA (Must be FIRST) 
        CSG_TAG_SCREENCOUNT, (void *)SCREENS,   // How many screens to make! 
        CSG_TAG_BITMAPCOUNT,(void *)1,
        CSG_TAG_BITMAPHEIGHT_ARRAY,(void *)&gHeightArray[0],
        CSG_TAG_DONE, 0         // End of list 
    };
#endif

// DC: TODO: unused currently
#if 0
static char gFileName[32];
#endif

void InitTools()
{
    uint32_t i;     // Temp 

    // DC: 3DO specific - disabling
    #if 0
        long width, height;         // Screen width & height 
        struct Screen *screen;      // Pointer to screen info 
        struct ItemNode *Node;
        Item MyVDLItem;             // Read page PRF-85 for info 
    
        #if 1
            Show3DOLogo();              // Show the 3DO Logo 
            RunAProgram("IdLogo IDLogo.cel");
        #if 1           // Set to 1 for Japanese version 
            RunAProgram("IdLogo LogicLogo.cel");
            RunAProgram("PlayMovie EALogo.cine");
            RunAProgram("IdLogo AdiLogo.cel");
        #else
            RunAProgram("PlayMovie Logic.cine");
            RunAProgram("PlayMovie AdiLogo.cine");
        #endif
        #endif
    
        if (OpenGraphicsFolio() ||  // Start up the graphics system 
            (OpenAudioFolio()<0) ||     // Start up the audio system 
            (OpenMathFolio()<0) )
        {
        FooBar:
            exit(10);
        }
    #endif
    
    #if 0 // Set to 1 for the PAL version, 0 for the NTSC version 
        QueryGraphics(QUERYGRAF_TAG_DEFAULTDISPLAYTYPE,&width);
        if (width==DI_TYPE_NTSC) {
            goto FooBar();
        }
    #endif

    #if 0 // Remove for final build! 
        ChangeDirectory("/CD-ROM");
    #endif

    // DC: 3DO specific code - disabling
    #if 0
        gScreenTags[0].ta_Arg = (void *)GETBANKBITS(GrafBase->gf_ZeroPage);
        gScreenGroupItem = CreateScreenGroup(gScreenItems,gScreenTags);

        if (gScreenGroupItem<0) {        // Error creating screens? 
            goto FooBar;
        }
        AddScreenGroup(gScreenGroupItem,NULL);       // Add my screens to the system 

        screen = (Screen*)LookupItem(gScreenItems[0]);
        if (!screen) {
            goto FooBar;
        }

        width = screen->scr_TempBitmap->bm_Width;       // How big is the screen? 
        height = screen->scr_TempBitmap->bm_Height;

        gScreenPageCount = (width*2*height+GrafBase->gf_VRAMPageSize-1)/GrafBase->gf_VRAMPageSize;
        gScreenByteCount = gScreenPageCount * GrafBase->gf_VRAMPageSize;

        i=0;
        do {        // Process the screens 
            screen = (Screen *)LookupItem(gScreenItems[i]);
            gScreenMaps[i] = (Byte *)screen->scr_TempBitmap->bm_Buffer;
            memset(gScreenMaps[i],0,gScreenByteCount);    // Clear the screen 
            Node = (ItemNode *) screen->scr_TempBitmap; // Get the bitmap pointer 
            gVideoItems[i] = (Item)Node->n_Item;         // Get the bitmap item # 
            gMyCustomVDL[9]=gMyCustomVDL[10] = (Word)gScreenMaps[i];
            MyVDLItem = SubmitVDL((VDLEntry *)&gMyCustomVDL[0],sizeof(gMyCustomVDL)/4,VDLTYPE_FULL);
            SetVDL(gScreenItems[i],MyVDLItem);

            SetClipWidth(gVideoItems[i],320);
            SetClipHeight(gVideoItems[i],200);       // I only want 200 lines 
            SetClipOrigin(gVideoItems[i],0,0);       // Set the clip top for the screen 
        } while (++i<SCREENS);

        InitEventUtility(1, 1, false);    // I want 1 joypad, 1 mouse, and passive listening 
    #endif

    audioInit();

    // DC: 3DO specific code - disabling
    #if 0
        gMainTask = KernelBase->kb_CurrentTask->t.n_Item;    // My task Item 
        gVRAMIOReq = GetVRAMIOReq();
        SetMyScreen(0);                                     // Init the video display 
    #endif

    audioLoadAllSounds();
    resourcesInit();
    
    {
        MyCCB *CCBPtr;
        i = CCBTotal;
        CCBPtr = gCCBArray;
        
        do {
            CCBPtr->ccb_NextPtr = (MyCCB *)(sizeof(MyCCB)-8);   // Create the next offset 
            CCBPtr->ccb_HDDX = 0;   // Set the defaults 
            CCBPtr->ccb_HDDY = 0;
            ++CCBPtr;
        } while (--i);
    }
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

    Read a file from NVRAM or a memory card into buf

**********************************/

// DC: 3DO specific - remove eventually
#if 0
    static LongWord gRamFileSize;
#endif

int32_t StdReadFile(char *fName,char *buf)
{
    // DC: FIXME: Implement File I/O
    #if 0
        int32 err;          // Error code to return 
        Item fd;            // Disk file referance 
        Item req;           // IO request item 
        IOReq *reqp;        // Pointer to IO request item 
        IOInfo params;      // Parameter list for I/O information 
        DeviceStatus ds;    // Struct for device status 
    
        fd = OpenDiskFile(fName);   // Open the file 
        if (fd < 0) {               // Error? 
            return fd;
        }
    
        req = CreateIOReq(NULL,0,fd,0);         // Create an I/O item 
        reqp = (IOReq *)LookupItem(req);        // Deref the item pointer 
        memset(&params,0,sizeof(IOInfo));       // Blank the I/O record 
        memset(&ds,0,sizeof(DeviceStatus));     // Blank the device status 
        params.ioi_Command = CMD_STATUS;        // Make a status command 
        params.ioi_Recv.iob_Buffer = &ds;       // Set the I/O buffer ptr 
        params.ioi_Recv.iob_Len = sizeof(DeviceStatus); // Set the length 
        err = DoIO(req,&params);                // Perform the status I/O 
        if (err>=0) {           // Status ok? 
            // Try to read it in 

            // Calc the read size based on blocks 
            gRamFileSize = ds.ds_DeviceBlockCount * ds.ds_DeviceBlockSize;
            memset(&params,0,sizeof(IOInfo));       // Zap the I/O info record 
            params.ioi_Command = CMD_READ;          // Read command 
            params.ioi_Recv.iob_Len = gRamFileSize;  // Data length 
            params.ioi_Recv.iob_Buffer = buf;       // Data buffer 
            err = DoIO(req,&params);                // Read the file 
        }
        DeleteIOReq(req);       // Release the IO request 
        CloseDiskFile(fd);      // Close the disk file 
        return err;             // Return the error code (If any) 
    #else
        return -1;
    #endif
}

/**********************************

    Write out the prefs to the NVRAM

**********************************/

#define PREFWORD 0x4C57
static char gPrefsName[] = "/NVRAM/DoomPrefs";       // Save game name 

void WritePrefsFile()
{
    uint32_t PrefFile[10];      // Must match what's in ReadPrefsFile!! 
    uint32_t CheckSum;          // Checksum total 
    uint32_t i;

    PrefFile[0] = PREFWORD;
    PrefFile[1] = gStartSkill;
    PrefFile[2] = gStartMap;
    PrefFile[3] = audioGetSoundVolume();
    PrefFile[4] = audioGetMusicVolume();
    PrefFile[5] = gControlType;
    PrefFile[6] = gMaxLevel;
    PrefFile[7] = gScreenSize;
    PrefFile[8] = 0;            // Was 'LowDetail' - now unused
    PrefFile[9] = 12345;        // Init the checksum 
    i = 0;
    CheckSum = 0;
    do {
        CheckSum += PrefFile[i];        // Make a simple checksum 
    } while (++i<10);
    PrefFile[9] = CheckSum;
    SaveAFile(gPrefsName, &PrefFile, sizeof(PrefFile));    // Save the game file 
}

//-------------------------------------------------------------------------------------------------
// Clear out the prefs file
//-------------------------------------------------------------------------------------------------
void ClearPrefsFile() {
    gStartSkill = sk_medium;                    // Init the basic skill level
    gStartMap = 1;                              // Only allow playing from map #1
    audioSetSoundVolume(MAX_AUDIO_VOLUME);      // Init the sound effects volume
    audioSetMusicVolume(MAX_AUDIO_VOLUME);      // Init the music volume
    gControlType = 3;                           // Use basic joypad controls
    gMaxLevel = 1;                              // Only allow level 1 to select from
    gScreenSize = 0;                            // Default screen size
    WritePrefsFile();                           // Output the new prefs
}

/**********************************

    Load in the standard prefs

**********************************/

void ReadPrefsFile()
{
    uint32_t PrefFile[88];      // Must match what's in WritePrefsFile!! 
    uint32_t CheckSum;          // Running checksum 
    uint32_t i;

    if (StdReadFile(gPrefsName,(char *) PrefFile)<0) {   // Error reading? 
        ClearPrefsFile();       // Clear it out 
        return;
    }

    i = 0;
    CheckSum = 12345;       // Init the checksum 
    do {
        CheckSum+=PrefFile[i];  // Calculate the checksum 
    } while (++i<9);

    if ((CheckSum != PrefFile[10-1]) || (PrefFile[0] !=PREFWORD)) {
        ClearPrefsFile();   // Bad ID or checksum! 
        return;
    }
    
    gStartSkill = (skill_e)PrefFile[1];
    gStartMap = PrefFile[2];
    audioSetSoundVolume(PrefFile[3]);
    audioSetMusicVolume(PrefFile[4]);
    gControlType = PrefFile[5];
    gMaxLevel = PrefFile[6];
    gScreenSize = PrefFile[7];
    
    if ((gStartSkill >= (sk_nightmare+1)) ||
        (gStartMap >= 27) ||
        (gControlType >= 6) ||
        (gMaxLevel >= 26) ||
        (gScreenSize >= 6)
    ) {
        ClearPrefsFile();
    }
}

/**********************************

    Flush all the cached CCB's

**********************************/

static void FlushCCBs()
{
    // DC: 3DO specific code - disabling
    #if 0
        MyCCB* NewCCB;

        NewCCB = CurrentCCB;
        if (NewCCB!=&gCCBArray[0]) {
            --NewCCB;       // Get the last used CCB 
            NewCCB->ccb_Flags |= CCB_LAST;  // Mark as the last one 
            DrawCels(VideoItem,(CCB *)&gCCBArray[0]);    // Draw all the cels in one shot 
            CurrentCCB = &gCCBArray[0];      // Reset the empty entry 
        }
        gSpanPtr = gSpanArray;        // Reset the floor texture pointer 
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
            NewImage = VideoPointer;    // Pointer to the NEW image 
            PrevPage = gWorkPage-1;  // Get the currently displayed page 
            if (PrevPage==-1) {     // Wrapped? 
                PrevPage = SCREENS-1;
            }
            SetMyScreen(PrevPage);      // Set videopointer to display buffer 
            if (!PrevPage) {
                PrevPage=SCREENS;
            }
            --PrevPage;
            OldImage = (Byte *) &gScreenMaps[PrevPage][0];   // Get work buffer 
            
                // Copy the buffer from display to work 
            memcpy(OldImage,VideoPointer,320*200*2);
            WipeDoom((LongWord *)OldImage,(LongWord *)NewImage);            // Perform the wipe 
        }
        DisplayScreen(gScreenItems[gWorkPage],0);     // Display the hidden page 
        if (++gWorkPage>=SCREENS) {      // Next screen in line 
            gWorkPage = 0;
        }
        SetMyScreen(gWorkPage);      // Set the 3DO vars 
        do {
            NewTick = ReadTick();               // Get the time mark 
            LastTics = NewTick - gLastTicCount;  // Get the time elapsed 
        } while (!LastTics);                    // Hmmm, too fast?!?!? 
        gLastTicCount = NewTick;                 // Save the time mark 
    #endif
}

/**********************************

    Draw a shape centered on the screen
    Used for "Loading or Paused" pics

**********************************/

void DrawPlaque(uint32_t RezNum)
{
    uint32_t PrevPage;
    PrevPage = gWorkPage-1;
    if (PrevPage==-1) {
        PrevPage = SCREENS-1;
    }
    FlushCCBs();        // Flush pending draws 
    SetMyScreen(PrevPage);      // Draw to the active screen 
    const CelControlBlock* const pPic = (const CelControlBlock*) loadResourceData(RezNum);
    DrawShape(160 - (getCCBWidth(pPic) / 2), 80, pPic);
    FlushCCBs();        // Make sure it's drawn 
    releaseResource(RezNum);
    SetMyScreen(gWorkPage);      // Reset to normal 
}

/**********************************

    Draw a scaled solid line

**********************************/

void AddCCB(uint32_t x, uint32_t y, MyCCB* NewCCB)
{
    // DC: FIXME: implement/replace
    #if 0
        MyCCB* DestCCB;         // Pointer to new CCB entry 
        LongWord TheFlags;      // CCB flags 
        LongWord ThePtr;        // Temp pointer to munge 

        DestCCB = CurrentCCB;       // Copy pointer to local 
        if (DestCCB>=&gCCBArray[CCBTotal]) {     // Am I full already? 
            FlushCCBs();
            DestCCB = gCCBArray;
        }
        TheFlags = NewCCB->ccb_Flags;       // Preload the CCB flags 
        DestCCB->ccb_XPos = x<<16;      // Set the x and y coord 
        DestCCB->ccb_YPos = y<<16;
        DestCCB->ccb_HDX = NewCCB->ccb_HDX; // Set the data for the CCB 
        DestCCB->ccb_HDY = NewCCB->ccb_HDY;
        DestCCB->ccb_VDX = NewCCB->ccb_VDX;
        DestCCB->ccb_VDY = NewCCB->ccb_VDY;
        DestCCB->ccb_PIXC = NewCCB->ccb_PIXC;
        DestCCB->ccb_PRE0 = NewCCB->ccb_PRE0;
        DestCCB->ccb_PRE1 = NewCCB->ccb_PRE1;

        ThePtr = (LongWord)NewCCB->ccb_SourcePtr;   // Force absolute address 
        if (!(TheFlags&CCB_SPABS)) {
            ThePtr += ((LongWord)NewCCB)+12;    // Convert relative to abs 
        }
        DestCCB->ccb_SourcePtr = (CelData *)ThePtr; // Save the source ptr 

        if (TheFlags&CCB_LDPLUT) {      // Only load a new plut if REALLY needed 
            ThePtr = (LongWord)NewCCB->ccb_PLUTPtr;
            if (!(TheFlags&CCB_PPABS)) {        // Relative plut? 
                ThePtr += ((LongWord)NewCCB)+16;    // Convert relative to abs 
            }
            DestCCB->ccb_PLUTPtr = (void *)ThePtr;  // Save the PLUT pointer 
        }
        DestCCB->ccb_Flags = (TheFlags & ~(CCB_LAST|CCB_NPABS)) | (CCB_SPABS|CCB_PPABS);
        ++DestCCB;              // Next CCB 
        CurrentCCB = DestCCB;   // Save the CCB pointer 
    #endif
}

static void DrawShapeImpl(const uint32_t x1, const uint32_t y1, const CelControlBlock* const pShape, bool bIsMasked) noexcept {
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

void DrawMShape(const uint32_t x1, const uint32_t y1, const CelControlBlock* const pShape) noexcept {
    // TODO: DC - This is temp!
    DrawShapeImpl(x1, y1, pShape, true);

    // DC: FIXME: implement/replace
    #if 0
        ((MyCCB*)ShapePtr)->ccb_Flags &= ~CCB_BGND;     // Enable masking 
        AddCCB(x,y,(MyCCB*)ShapePtr);                   // Draw the shape 
    #endif
}

/**********************************

    Draw an unmasked shape on the screen

**********************************/

void DrawShape(const uint32_t x1, const uint32_t y1, const CelControlBlock* const pShape) noexcept {
    // TODO: DC - This is temp!
    DrawShapeImpl(x1, y1, pShape, false);

    // DC: FIXME: implement/replace
    #if 0
        ((MyCCB*)ShapePtr)->ccb_Flags |= CCB_BGND;      // Disable masking 
        AddCCB(x,y,(MyCCB*)ShapePtr);                   // Draw the shape 
    #endif
}

/**********************************

    This code is functionally equivalent to the Burgerlib
    version except that it is using the cached CCB system.

**********************************/

void DrawARect(const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height, const uint16_t color) noexcept {

    const uint32_t color32 = Video::rgba5551ToScreenCol(color);

    // Clip the rect bounds
    if (x >= Video::SCREEN_WIDTH || y >= Video::SCREEN_HEIGHT)
        return;
    
    const uint32_t xEnd = std::min(x + width, Video::SCREEN_WIDTH);
    const uint32_t yEnd = std::min(y + height, Video::SCREEN_HEIGHT);

    // Fill the color
    uint32_t* pRow = Video::gFrameBuffer + x + (y * Video::SCREEN_WIDTH);

    for (uint32_t yCur = y; yCur < yEnd; ++yCur) {
        uint32_t* pPixel = pRow;

        for (uint32_t xCur = x; xCur < xEnd; ++xCur) {
            *pPixel = color32;
            ++pPixel;
        }

        pRow += Video::SCREEN_WIDTH;
    }
}

/**********************************

    Set the hardware clip rect to the actual game screen
    
**********************************/

void EnableHardwareClipping()
{
    // DC: FIXME: implement/replace
    #if 0
        FlushCCBs();                                            // Failsafe 
        SetClipWidth(VideoItem,ScreenWidth);
        SetClipHeight(VideoItem,ScreenHeight);                  // I only want 200 lines 
        SetClipOrigin(VideoItem,ScreenXOffset,ScreenYOffset);   // Set the clip top for the screen 
    #endif
}

/**********************************

    Restore the clip rect to normal
    
**********************************/

void DisableHardwareClipping()
{
    // DC: FIXME: implement/replace
    #if 0
        FlushCCBs();                        // Failsafe 
        SetClipOrigin(VideoItem,0,0);       // Set the clip top for the screen 
        SetClipWidth(VideoItem,320);
        SetClipHeight(VideoItem,200);       // I only want 200 lines 
    #endif
}

#if 0
/**********************************

    This will allow screen shots to be taken.
    REMOVE FOR FINAL BUILD!!!

**********************************/

Word gLastJoyButtons[4];     // Save the previous joypad bits 
static Word gFileNum;
static uint16_t gOneLine[640];

Word ReadJoyButtons(Word PadNum)
{
    char gFileName[20];
    ControlPadEventData ControlRec;
    uint16_t *OldImage;
    uint16_t *DestImage;

    GetControlPad(PadNum+1,FALSE,&ControlRec);      // Read joypad 
    if (PadNum<4) {
        if (((ControlRec.cped_ButtonBits^gLastJoyButtons[PadNum]) &
            ControlRec.cped_ButtonBits)&PadC) {
            Word i,j,PrevPage;
            
            sprintf(gFileName,"Art%d.RAW16",gFileNum);
            ++gFileNum;
            PrevPage = gWorkPage-1;  // Get the currently displayed page 
            if (PrevPage==-1) {     // Wrapped? 
                PrevPage = SCREENS-1;
            }
            OldImage = (uint16_t *) &gScreenMaps[PrevPage][0];  // Get work buffer 
            i = 0;
            DestImage = OldImage;
            do {
                memcpy(gOneLine,DestImage,320*2*2);
                j = 0;
                do {
                    DestImage[j] = gOneLine[j*2];
                    DestImage[j+320] = gOneLine[(j*2)+1];
                } while (++j<320);
                DestImage+=640;
            } while (++i<100);
            WriteMacFile(gFileName,OldImage,320*200*2);
        }
        gLastJoyButtons[PadNum] = (Word)ControlRec.cped_ButtonBits;
    }
    return (Word)ControlRec.cped_ButtonBits;        // Return the data 
}
#endif
