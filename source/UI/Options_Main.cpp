#include "Options_Main.h"

#include "Audio/Audio.h"
#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "GFX/CelImages.h"
#include "GFX/Renderer.h"
#include "GFX/Video.h"
#include "Intermission_Main.h"
#include "ThreeDO.h"

#define CURSORX 45      // X coord for skulls 
#define SLIDERX 106     // X coord for slider bars 
#define SLIDESTEP 6     // Adjustment for volume to screen coord 
#define JOYPADX 90      // X coord for joypad text 

#define SFXVOLY 60      // Y coord for SFX volume control 
#define MUSICVOLY 120   // Y coord for Music volume control 
#define JOYPADY 40      // Y coord for joypad control 
#define SIZEY 140       // Y coord for screen size control 

// Action buttons can be set to PadA, PadB, or PadC 

enum {      // Control option count 
    SFU,SUF,FSU,FUS,USF,UFS,NUMCONTROLOPTIONS
};

enum {      // Menu items to select from 
    soundvol,       // Volume 
    musicvol,       // Music volume 
    controls,       // Control settings 
    size,           // Screen size settings 
    NUMMENUITEMS
};

enum {
    BAR,        // Sub shapes for scroll bar 
    HANDLE
};

static uint32_t gCursorFrame;       // Skull animation frame 
static uint32_t gCursorCount;       // Time mark to animate the skull 
static uint32_t gCursorPos;         // Y position of the skull 
static uint32_t gMoveCount;         // Time mark to move the skull 

static constexpr uint32_t CURSOR_Y_POS[NUMMENUITEMS] = {
    SFXVOLY - 2,
    MUSICVOLY - 2,
    JOYPADY - 2,
    SIZEY - 2
};

static constexpr const char* const SPEED_TEXT = "Speed";       // Local ASCII
static constexpr const char* const FIRE_TEXT = "Fire";
static constexpr const char* const USE_TEXT = "Use";

static constexpr const char* const BUTTON_A[NUMCONTROLOPTIONS] = {
    SPEED_TEXT,
    SPEED_TEXT,
    FIRE_TEXT,
    FIRE_TEXT,
    USE_TEXT,
    USE_TEXT
};

static constexpr const char* const BUTTON_B[NUMCONTROLOPTIONS] = {
    FIRE_TEXT,
    USE_TEXT,
    SPEED_TEXT,
    USE_TEXT,
    SPEED_TEXT,
    FIRE_TEXT
};

static constexpr const char* const BUTTON_C[NUMCONTROLOPTIONS] = {
    USE_TEXT,
    FIRE_TEXT,
    USE_TEXT,
    SPEED_TEXT,
    FIRE_TEXT,
    SPEED_TEXT
};

static constexpr uint32_t CONFIGURATION[NUMCONTROLOPTIONS][3] = {
    { PadA, PadB, PadC },
    { PadA, PadC, PadB },
    { PadB, PadA, PadC },
    { PadC, PadA, PadB },
    { PadB, PadC, PadA },
    { PadC, PadB, PadA }
};

/**********************************

    Init the button settings from the control type

**********************************/

static void SetButtonsFromControltype(void)
{
    const uint32_t *TablePtr;

    TablePtr = &CONFIGURATION[gControlType][0];  // Init table 
    gPadSpeed = TablePtr[0];     // Init the joypad settings 
    gPadAttack = TablePtr[1];
    gPadUse = TablePtr[2];
    Renderer::initMathTables();     // Handle the math tables 
}

/**********************************

    Init the option screens
    Called on powerup.

**********************************/

void O_Init() noexcept {
// The prefs has set controltype, so set buttons from that 

    SetButtonsFromControltype();        // Init the joypad settings 
    gCursorCount = 0;        // Init skull cursor state 
    gCursorFrame = 0;
    gCursorPos = 0;
}

/**********************************

    Button bits can be eaten by clearing them in JoyPadButtons
    Called by player code.
    
**********************************/

void O_Control(player_t *player) noexcept {
    uint32_t buttons;
    buttons = gJoyPadButtons;

    if (gNewJoyPadButtons & PadX) {      // Toggled the option screen? 
        if (player) {
            player->AutomapFlags ^= AF_OPTIONSACTIVE;   // Toggle the flag 
            if (!(player->AutomapFlags & AF_OPTIONSACTIVE)) {   // Shut down? 
                SetButtonsFromControltype();    // Set the memory 
                WritePrefsFile();       // Save new settings to NVRAM 
            }
        } else {
            SetButtonsFromControltype();    // Set the memory 
            WritePrefsFile();       // Save new settings to NVRAM 
        }
    }
    
    if (player) {
        if ( !(player->AutomapFlags & AF_OPTIONSACTIVE) ) { // Can I execute? 
            return;     // Exit NOW! 
        }
    }
    
// Clear buttons so game player isn't moving around 

    gJoyPadButtons = buttons&PadX;   // Leave option status alone 

    // animate skull 
    ++gCursorCount;
    if (gCursorCount >= (TICKSPERSEC/4)) {   // Time up? 
        gCursorFrame ^= 1;       // Toggle the frame 
        gCursorCount = 0;        // Reset the timer 
    }

    // Check for movement
    if (! (buttons & (PadUp|PadDown|PadLeft|PadRight) ) ) {
        gMoveCount = TICKSPERSEC;        // move immediately on next press
    } else {
        ++gMoveCount;
        if ( (gMoveCount >= (TICKSPERSEC/3)) ||      // Allow slow 
            (gCursorPos < controls && gMoveCount >= (TICKSPERSEC/5))) {   // Fast? 
            gMoveCount = 0;      // Reset timer
            
            // Try to move the cursor up or down...            
            if (buttons & PadDown) {        
                ++gCursorPos;
                if (gCursorPos >= NUMMENUITEMS) {
                    gCursorPos = 0;
                }
            }
            if (buttons & PadUp) {
                if (!gCursorPos) {
                    gCursorPos = NUMMENUITEMS;
                }
                --gCursorPos;
            }
            
            switch (gCursorPos) {        // Adjust the control 
            case controls:          // Joypad?    
                if (buttons & PadRight) {
                    if (gControlType<(NUMCONTROLOPTIONS-1)) {
                        ++gControlType;
                    }
                }
                if (buttons & PadLeft) {
                    if (gControlType) {
                        --gControlType;
                    }
                }
                break;
            
            // Sound volume?
            case soundvol: {
                if (buttons & PadRight) {
                    const uint32_t soundVolume = audioGetSoundVolume();

                    if (soundVolume < MAX_AUDIO_VOLUME) {
                        audioSetSoundVolume(soundVolume + 1);
                        S_StartSound(0, sfx_pistol);
                    }
                }

                if (buttons & PadLeft) {
                    const uint32_t soundVolume = audioGetSoundVolume();

                    if (soundVolume > 0) {
                        audioSetSoundVolume(soundVolume - 1);
                        S_StartSound(0, sfx_pistol);
                    }
                }
            }   break;

            // Music volume?
            case musicvol: {
                if (buttons & PadRight) {
                    const uint32_t musicVolume = audioGetMusicVolume();

                    if (musicVolume < MAX_AUDIO_VOLUME) {
                        audioSetMusicVolume(musicVolume + 1);
                    }
                }

                if (buttons & PadLeft) {
                    const uint32_t musicVolume = audioGetMusicVolume();

                    if (musicVolume > 0) {
                        audioSetMusicVolume(musicVolume - 1);
                    }
                }
            }   break;
            
            case size:          // Screen size 
                if (buttons & PadLeft)  {
                    if (gScreenSize<(6-1)) {
                        ++gScreenSize;
                        if (player) {
                            Renderer::initMathTables();     // Handle the math tables 
                        }
                    }
                }
                if (buttons & PadRight) {
                    if (gScreenSize > 0) {       // Can it grow? 
                        --gScreenSize;
                        if (player) {
                            Renderer::initMathTables();     // Handle the math tables 
                        }
                    }
                }
            }   
        }
    }
}

/**********************************

    Draw the option screen

**********************************/
void O_Drawer(const bool bSaveFrameBuffer) noexcept {
    // Erase old and Draw new cursor frame
    const CelImageArray& skullImgs = CelImages::loadImages(rSKULLS, CelImages::LoadFlagBits::MASKED);
    Renderer::drawUISprite(CURSORX, CURSOR_Y_POS[gCursorPos], skullImgs.getImage(gCursorFrame));
    CelImages::releaseImages(rSKULLS);

    // Draw menu text
    const CelImageArray& sliderImgs = CelImages::loadImages(rSLIDER, CelImages::LoadFlagBits::MASKED);
    PrintBigFontCenter(160, 10, "Options");

    if (gCursorPos < controls) {
        PrintBigFontCenter(160, SFXVOLY, "Sound Volume");
        PrintBigFontCenter(160, MUSICVOLY, "Music Volume");

        // Draw scroll bars
        Renderer::drawUISprite(SLIDERX, SFXVOLY + 20, sliderImgs.getImage(BAR));
        Renderer::drawUISprite(SLIDERX, MUSICVOLY + 20, sliderImgs.getImage(BAR));
        
        {
            const uint32_t offset = audioGetSoundVolume() * SLIDESTEP;
            Renderer::drawUISprite(SLIDERX + 5 + offset, SFXVOLY + 20, sliderImgs.getImage(HANDLE));
        }

        {
            const uint32_t offset = audioGetMusicVolume() * SLIDESTEP;
            Renderer::drawUISprite(SLIDERX + 5 + offset, MUSICVOLY + 20, sliderImgs.getImage(HANDLE));
        }

    } else {
        // Draw joypad info
        PrintBigFontCenter(160, JOYPADY, "Controls");
        PrintBigFont(JOYPADX + 10, JOYPADY + 20, "A");
        PrintBigFont(JOYPADX + 10, JOYPADY + 40, "B");
        PrintBigFont(JOYPADX + 10, JOYPADY + 60, "C");
        PrintBigFont(JOYPADX + 40, JOYPADY + 20, BUTTON_A[gControlType]);
        PrintBigFont(JOYPADX + 40, JOYPADY + 40, BUTTON_B[gControlType]);
        PrintBigFont(JOYPADX + 40, JOYPADY + 60, BUTTON_C[gControlType]);
        PrintBigFontCenter(160, SIZEY, "Screen Size");
        Renderer::drawUISprite(SLIDERX, SIZEY + 20, sliderImgs.getImage(BAR));
        
        const uint32_t offset = (5 - gScreenSize) * 18;
        Renderer::drawUISprite(SLIDERX + 5 + offset, SIZEY + 20, sliderImgs.getImage(HANDLE));
    }

    CelImages::releaseImages(rSLIDER);
    Video::present(bSaveFrameBuffer);
}
