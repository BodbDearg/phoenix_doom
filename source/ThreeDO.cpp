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

/**********************************

    This contains all the 3DO specific calls for Doom
    
**********************************/

// DC: TODO: unused currently
#if 0
static void WipeDoom(LongWord *OldScreen,LongWord *NewScreen);
#endif

// DC: TODO: unused currently
#if 0
uint8_t gSpanArray[MAXSCREENWIDTH*MAXSCREENHEIGHT];     // Buffer for floor textures 
uint8_t* gSpanPtr = gSpanArray;                         // Pointer to empty buffer 
#endif

// DC: TODO: unused currently
#if 0
#define SCREENS 3                   // Need to page flip 
static Item gScreenItems[SCREENS];   // Referances to the game screens 
static Item gVideoItems[SCREENS];
static Byte *gScreenMaps[SCREENS];   // Pointer to the bitmap screens 
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
void InitTools() {
    // DC: 3DO specific - disabling
    #if 0
        #if 1
            Show3DOLogo();  // Show the 3DO Logo 
            RunAProgram("IdLogo IDLogo.cel");
            #if 1   // Set to 1 for Japanese version 
                RunAProgram("IdLogo LogicLogo.cel");
                RunAProgram("PlayMovie EALogo.cine");
                RunAProgram("IdLogo AdiLogo.cel");
            #else
                RunAProgram("PlayMovie Logic.cine");
                RunAProgram("PlayMovie AdiLogo.cine");
            #endif
        #endif
    #endif

    audioInit();

    // FIXME: REIMPLEMENT
    #if 0
        SetMyScreen(0);     // Init the video display 
    #endif

    audioLoadAllSounds();
    Resources::init();
}

//---------------------------------------------------------------------------------------------------------------------
// Main entry point for 3DO
//---------------------------------------------------------------------------------------------------------------------
void ThreeDOMain() {
    InitTools();                // Init the 3DO tool system
    Video::init();
    ReadPrefsFile();            // Load defaults
    D_DoomMain();               // Start doom
    Video::shutdown();
}

int32_t StdReadFile(const char* const fName, char* buf)
{
    // DC: FIXME: Implement File I/O
    return -1;
}

/**********************************

    Write out the prefs to the NVRAM

**********************************/
static constexpr char gPrefsName[] = "/NVRAM/DoomPrefs";       // Save game name 
static constexpr uint32_t PREFWORD = 0x4C57;

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
    gMaxLevel = 24;                             // Only allow level 1 to select from
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

    Draw a shape centered on the screen
    Used for "Loading or Paused" pics

**********************************/
void DrawPlaque(uint32_t RezNum)
{
    // FIXME: DC: Required for screen wipe?
    #if 0
    uint32_t PrevPage;
    PrevPage = gWorkPage-1;
    if (PrevPage==-1) {
        PrevPage = SCREENS-1;
    }
    SetMyScreen(PrevPage);      // Draw to the active screen 
    #endif

    const CelControlBlock* const pPic = (const CelControlBlock*) Resources::loadData(RezNum);
    DrawShape(160 - (CelUtils::getCCBWidth(pPic) / 2), 80, pPic);
    Resources::release(RezNum);

    // FIXME: DC: Required for screen wipe?
    #if 0
    SetMyScreen(gWorkPage);      // Reset to normal 
    #endif
}

static void DrawShapeImpl(const uint32_t x1, const uint32_t y1, const CelControlBlock* const pShape, bool bIsMasked) noexcept {
    // TODO: DC - This is temp!
    uint16_t* pImage;
    uint16_t imageW;
    uint16_t imageH;
    CelUtils::decodeDoomCelSprite(pShape, &pImage, &imageW, &imageH);
    
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
    DrawShapeImpl(x1, y1, pShape, true);
}

/**********************************

    Draw an unmasked shape on the screen

**********************************/
void DrawShape(const uint32_t x1, const uint32_t y1, const CelControlBlock* const pShape) noexcept {
    DrawShapeImpl(x1, y1, pShape, false);
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
