#include "ThreeDO.h"

#include "Audio/Audio.h"
#include "Base/Endian.h"
#include "Base/Input.h"
#include "Base/Macros.h"
#include "Base/Mem.h"
#include "Base/Tables.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomMain.h"
#include "Game/Resources.h"
#include "GFX/CelImages.h"
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

//----------------------------------------------------------------------------------------------------------------------
// Setup various game subsystems (audio, video input etc.)
//----------------------------------------------------------------------------------------------------------------------
void initGameSubsystems() noexcept {
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

    // FIXME: REIMPLEMENT
    #if 0
        SetMyScreen(0);     // Init the video display 
    #endif

    Resources::init();
    CelImages::init();
    Video::init();
    Input::init();
    audioInit();
    audioLoadAllSounds();
}

//----------------------------------------------------------------------------------------------------------------------
// Shut down various game subsystems (audio, video input etc.)
//----------------------------------------------------------------------------------------------------------------------
void shutdownGameSubsystems() noexcept {
    audioShutdown();
    Input::shutdown();
    Video::shutdown();
    CelImages::shutdown();
    Resources::shutdown();
}

//---------------------------------------------------------------------------------------------------------------------
// Main entry point for 3DO
//---------------------------------------------------------------------------------------------------------------------
void ThreeDOMain() noexcept {
    initGameSubsystems();
    
    ReadPrefsFile();    // Load defaults
    D_DoomMain();       // Start doom

    shutdownGameSubsystems();
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
// TODO: MOVE ELSEWHERE
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

    const CelImage& img = CelImages::loadImage(RezNum);
    Renderer::drawUISprite(160 - img.width / 2, 80, img);
    CelImages::releaseImages(RezNum);

    // FIXME: DC: Required for screen wipe?
    #if 0
    SetMyScreen(gWorkPage);      // Reset to normal 
    #endif
}
