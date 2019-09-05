#include "Options_Main.h"

#include "Audio/Audio.h"
#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Input.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "GFX/CelImages.h"
#include "GFX/Renderer.h"
#include "GFX/Video.h"
#include "Intermission_Main.h"
#include "ThreeDO.h"

static constexpr uint32_t CURSORX       = 45;       // X coord for skulls
static constexpr uint32_t SLIDERX       = 106;      // X coord for slider bars
static constexpr uint32_t SLIDESTEP     = 6;        // Adjustment for volume to screen coord
static constexpr uint32_t JOYPADX       = 90;       // X coord for joypad text
static constexpr uint32_t SFXVOLY       = 60;       // Y coord for SFX volume control
static constexpr uint32_t MUSICVOLY     = 120;      // Y coord for Music volume control
static constexpr uint32_t JOYPADY       = 40;       // Y coord for joypad control
static constexpr uint32_t SIZEY         = 140;      // Y coord for screen size control

// Menu items to select from
enum {
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

static constexpr uint32_t CURSOR_Y_POS[NUMMENUITEMS] = {
    SFXVOLY - 2,
    MUSICVOLY - 2,
    JOYPADY - 2,
    SIZEY - 2
};

// TODO: REMOVE
static constexpr uint32_t NUMCONTROLOPTIONS = 6;

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

static uint32_t gCursorFrame;       // Skull animation frame
static uint32_t gCursorCount;       // Time mark to animate the skull
static uint32_t gCursorPos;         // Y position of the skull
static uint32_t gMoveCount;         // Time mark to move the skull

//----------------------------------------------------------------------------------------------------------------------
// Init the button settings from the control type.
//
// TODO: REMOVE THIS!
//----------------------------------------------------------------------------------------------------------------------
static void SetButtonsFromControltype() noexcept {
    Renderer::initMathTables();
}

//----------------------------------------------------------------------------------------------------------------------
// Init the option screens: called on powerup.
//----------------------------------------------------------------------------------------------------------------------
void O_Init() noexcept {
    // Init the joypad settings: the prefs has set controltype, so set buttons from that
    SetButtonsFromControltype();

    // Init skull cursor state
    gCursorCount = 0;
    gCursorFrame = 0;
    gCursorPos = 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Button bits can be eaten by clearing them in JoyPadButtons.
// Called by player code.
//----------------------------------------------------------------------------------------------------------------------
void O_Control(player_t* const pPlayer) noexcept {
    // Toggled the option screen?
    // Note that this is only allowed if the automap is not showing.
    if (MENU_ACTION_ENDED(BACK)) { 
        if (pPlayer) {
            if (!pPlayer->isAutomapActive()) {
                pPlayer->AutomapFlags ^= AF_OPTIONSACTIVE;      // Toggle the flag
                if (!pPlayer->isOptionsMenuActive()) {          // Shut down?
                    SetButtonsFromControltype();                // Set the memory
                    WritePrefsFile();                           // Save new settings to NVRAM
                }
            }
        } else {
            SetButtonsFromControltype();        // Set the memory
            WritePrefsFile();                   // Save new settings to NVRAM
        }
    }

    if (pPlayer) {
        if (!pPlayer->isOptionsMenuActive()) {      // Can I execute?
            return;                                 // Exit NOW!
        }
    }

    // Animate skull
    ++gCursorCount;

    if (gCursorCount >= TICKSPERSEC / 4) {      // Time up?
        gCursorFrame ^= 1;                      // Toggle the frame
        gCursorCount = 0;                       // Reset the timer
    }

    // Check for movement
    if (!Input::areAnyKeysOrButtonsPressed()) {
        // Move immediately on next press if nothing is pressed
        gMoveCount = TICKSPERSEC;
    } else {
        ++gMoveCount;

        if ((gMoveCount >= TICKSPERSEC / 3) || (    // Allow slow
                (gCursorPos < controls) && 
                (gMoveCount >= TICKSPERSEC / 5)     // Fast?
            )
        ) {
            gMoveCount = 0;      // Reset timer

            // Try to move the cursor up or down...
            if (MENU_ACTION(DOWN)) {
                ++gCursorPos;
                if (gCursorPos >= NUMMENUITEMS) {
                    gCursorPos = 0;
                }
            }

            if (MENU_ACTION(UP)) {
                if (gCursorPos <= 0) {
                    gCursorPos = NUMMENUITEMS;
                }
                --gCursorPos;
            }

            switch (gCursorPos) {   // Adjust the control
                // Joypad?
                case controls: {
                    if (MENU_ACTION(RIGHT)) {
                        if (gControlType < NUMCONTROLOPTIONS - 1) {
                            ++gControlType;
                        }
                    }

                    if (MENU_ACTION(LEFT)) {
                        if (gControlType > 0) {
                            --gControlType;
                        }
                    }
                }   break;

                // Sound volume?
                case soundvol: {
                    if (MENU_ACTION(RIGHT)) {
                        const uint32_t soundVolume = Audio::getSoundVolume();
                        if (soundVolume < Audio::MAX_VOLUME) {
                            Audio::setSoundVolume(soundVolume + 1);
                            S_StartSound(0, sfx_pistol);
                        }
                    }

                    if (MENU_ACTION(LEFT)) {
                        const uint32_t soundVolume = Audio::getSoundVolume();
                        if (soundVolume > 0) {
                            Audio::setSoundVolume(soundVolume - 1);
                            S_StartSound(0, sfx_pistol);
                        }
                    }
                }   break;

                // Music volume?
                case musicvol: {
                    if (MENU_ACTION(RIGHT)) {
                        const uint32_t musicVolume = Audio::getMusicVolume();
                        if (musicVolume < Audio::MAX_VOLUME) {
                            Audio::setMusicVolume(musicVolume + 1);
                        }
                    }

                    if (MENU_ACTION(LEFT)) {
                        const uint32_t musicVolume = Audio::getMusicVolume();
                        if (musicVolume > 0) {
                            Audio::setMusicVolume(musicVolume - 1);
                        }
                    }
                }   break;

                // Screen size
                case size: {
                    if (MENU_ACTION(RIGHT)) {
                        if (gScreenSize > 0) {
                            --gScreenSize;
                            if (pPlayer) {
                                Renderer::initMathTables();     // Handle the math tables
                            }
                        }
                    }

                    if (MENU_ACTION(LEFT)) {
                        if (gScreenSize < 6 - 1) {
                            ++gScreenSize;
                            if (pPlayer) {
                                Renderer::initMathTables();     // Handle the math tables
                            }
                        }
                    }
                }   break;
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Draw the option screen
//----------------------------------------------------------------------------------------------------------------------
void O_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    // Erase old and Draw new cursor frame
    const CelImageArray& skullImgs = CelImages::loadImages(rSKULLS, CelLoadFlagBits::MASKED);
    Renderer::drawUISprite(CURSORX, CURSOR_Y_POS[gCursorPos], skullImgs.getImage(gCursorFrame));
    CelImages::releaseImages(rSKULLS);

    // Draw menu text
    const CelImageArray& sliderImgs = CelImages::loadImages(rSLIDER, CelLoadFlagBits::MASKED);
    PrintBigFontCenter(160, 10, "Options");

    if (gCursorPos < controls) {
        PrintBigFontCenter(160, SFXVOLY, "Sound Volume");
        PrintBigFontCenter(160, MUSICVOLY, "Music Volume");

        // Draw scroll bars
        Renderer::drawUISprite(SLIDERX, SFXVOLY + 20, sliderImgs.getImage(BAR));
        Renderer::drawUISprite(SLIDERX, MUSICVOLY + 20, sliderImgs.getImage(BAR));

        {
            const uint32_t offset = Audio::getSoundVolume() * SLIDESTEP;
            Renderer::drawUISprite(SLIDERX + 5 + offset, SFXVOLY + 20, sliderImgs.getImage(HANDLE));
        }

        {
            const uint32_t offset = Audio::getMusicVolume() * SLIDESTEP;
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
    Video::endFrame(bPresent, bSaveFrameBuffer);
}
