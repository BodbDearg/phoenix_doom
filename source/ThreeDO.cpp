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

static int32_t StdReadFile(const char* const fName, char* buf) noexcept {
    // DC: FIXME: Implement File I/O
    return -1;
}

/**********************************

    Write out the prefs to the NVRAM

**********************************/
static constexpr char gPrefsName[] = "/NVRAM/DoomPrefs";       // Save game name
static constexpr uint32_t PREFWORD = 0x4C57;

void WritePrefsFile() noexcept {
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
void ClearPrefsFile() noexcept {
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
void ReadPrefsFile() noexcept {
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
void DrawPlaque(uint32_t RezNum) noexcept  {
    
    // FIXME: DC - The plaque causes a crash - is the image corrupt or are we reading it wrong?

    /*
    const CelImage& img = CelImages::loadImage(RezNum);
    Renderer::drawUISprite(160 - img.width / 2, 80, img);
    CelImages::releaseImages(RezNum);
    */
}
