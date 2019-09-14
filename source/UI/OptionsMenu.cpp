#include "OptionsMenu.h"

#include "Audio/Audio.h"
#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Input.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "Game/Prefs.h"
#include "Game/Tick.h"
#include "GFX/CelImages.h"
#include "GFX/Renderer.h"
#include "GFX/Video.h"
#include "IntermissionScreen.h"
#include "UIUtils.h"

static constexpr uint32_t CURSORX       = 45;       // X coord for skulls
static constexpr uint32_t SLIDERX       = 106;      // X coord for slider bars
static constexpr uint32_t SLIDESTEP     = 6;        // Adjustment for volume to screen coord
static constexpr uint32_t SFXVOLY       = 60;       // Y coord for SFX volume control
static constexpr uint32_t MUSICVOLY     = 120;      // Y coord for Music volume control
static constexpr uint32_t SIZEY         = 60;       // Y coord for screen size control
static constexpr uint32_t QUITY         = 120;      // Y coord for quit menu control

// Menu items to select from
enum {
    MENU_OPT_SOUND_VOL,     // Volume
    MENU_OPT_MUSIC_VOL,     // Music volume
    MENU_OPT_SCREEN_SIZE,   // Screen size settings
    MENU_OPT_QUIT,          // Quit option
    NUM_MENU_OPTIONS
};

enum {
    BAR,        // Sub shapes for scroll bar
    HANDLE
};

static constexpr uint32_t CURSOR_Y_POS[NUM_MENU_OPTIONS] = {
    SFXVOLY - 2,
    MUSICVOLY - 2,    
    SIZEY - 2,
    QUITY - 2,
};

static uint32_t gCursorFrame;       // Skull animation frame
static uint32_t gCursorCount;       // Time mark to animate the skull
static uint32_t gCursorPos;         // Y position of the skull
static uint32_t gMoveCount;         // Time mark to move the skull

//----------------------------------------------------------------------------------------------------------------------
// Init the option screens: called on powerup.
//----------------------------------------------------------------------------------------------------------------------
void O_Init() noexcept {
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
                    Prefs::save();                              // Save new settings
                } else {
                    O_Init();                                   // Reset option menu positions
                }
            }
        } else {
            Prefs::save();  // Save new settings
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

    // Handle the quit option
    if (gCursorPos == MENU_OPT_QUIT) {
        if (MENU_ACTION_ENDED(OK)) {
            if (gbIsPlayingMap) {
                gbQuitToMainRequested = true;
            } else {
                Input::requestQuit();
            }            
        }
    }

    // Check for movement
    if (!Input::areAnyKeysOrButtonsPressed()) {
        // Move immediately on next press if nothing is pressed
        gMoveCount = TICKSPERSEC;
    } else {
        ++gMoveCount;

        if ((gMoveCount >= TICKSPERSEC / 3) || (            // Allow slow
                (gCursorPos < MENU_OPT_SCREEN_SIZE) &&      // 1st or 2nd page?
                (gMoveCount >= TICKSPERSEC / 5)             // Fast?
            )
        ) {
            gMoveCount = 0;      // Reset timer

            // Gather up/down and left/right menu movements
            int menuMoveX = 0;
            int menuMoveY = 0;
            Controls::gatherAnalogAndDigitalMenuMovements(menuMoveX, menuMoveY);

            // Try to move the cursor up or down...
            if (menuMoveY >= 1) {
                ++gCursorPos;
                if (gCursorPos >= NUM_MENU_OPTIONS) {
                    gCursorPos = 0;
                }
            }
            else if (menuMoveY <= -1) {
                if (gCursorPos <= 0) {
                    gCursorPos = NUM_MENU_OPTIONS;
                }
                --gCursorPos;
            }

            // Handle inputs for each menu item
            switch (gCursorPos) {   
                // Sound volume?
                case MENU_OPT_SOUND_VOL: {
                    if (menuMoveX >= 1) {
                        const uint32_t soundVolume = Audio::getSoundVolume();
                        if (soundVolume < Audio::MAX_VOLUME) {
                            Audio::setSoundVolume(soundVolume + 1);
                            S_StartSound(0, sfx_pistol);
                        }
                    }
                    else if (menuMoveX <= -1) {
                        const uint32_t soundVolume = Audio::getSoundVolume();
                        if (soundVolume > 0) {
                            Audio::setSoundVolume(soundVolume - 1);
                            S_StartSound(0, sfx_pistol);
                        }
                    }
                }   break;

                // Music volume?
                case MENU_OPT_MUSIC_VOL: {
                    if (menuMoveX >= 1) {
                        const uint32_t musicVolume = Audio::getMusicVolume();
                        if (musicVolume < Audio::MAX_VOLUME) {
                            Audio::setMusicVolume(musicVolume + 1);
                        }
                    }
                    else if (menuMoveX <= -1) {
                        const uint32_t musicVolume = Audio::getMusicVolume();
                        if (musicVolume > 0) {
                            Audio::setMusicVolume(musicVolume - 1);
                        }
                    }
                }   break;

                // Screen size
                case MENU_OPT_SCREEN_SIZE: {
                    if (menuMoveX >= 1) {
                        if (gScreenSize > 0) {
                            --gScreenSize;
                            if (pPlayer) {
                                Renderer::initMathTables();     // Handle the math tables
                            }
                        }
                    }
                    else if (menuMoveX <= -1) {
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
    UIUtils::drawUISprite(CURSORX, CURSOR_Y_POS[gCursorPos], skullImgs.getImage(gCursorFrame));
    CelImages::releaseImages(rSKULLS);

    // Draw menu text
    const CelImageArray& sliderImgs = CelImages::loadImages(rSLIDER, CelLoadFlagBits::MASKED);
    PrintBigFontCenter(160, 10, "Options");

    if (gCursorPos < MENU_OPT_SCREEN_SIZE) {
        PrintBigFontCenter(160, SFXVOLY, "Sound Volume");
        PrintBigFontCenter(160, MUSICVOLY, "Music Volume");

        // Draw scroll bars
        UIUtils::drawUISprite(SLIDERX, SFXVOLY + 20, sliderImgs.getImage(BAR));
        UIUtils::drawUISprite(SLIDERX, MUSICVOLY + 20, sliderImgs.getImage(BAR));

        {
            const uint32_t offset = Audio::getSoundVolume() * SLIDESTEP;
            UIUtils::drawUISprite(SLIDERX + 5 + offset, SFXVOLY + 20, sliderImgs.getImage(HANDLE));
        }

        {
            const uint32_t offset = Audio::getMusicVolume() * SLIDESTEP;
            UIUtils::drawUISprite(SLIDERX + 5 + offset, MUSICVOLY + 20, sliderImgs.getImage(HANDLE));
        }

    } else {
        // Draw screen size slider
        PrintBigFontCenter(160, SIZEY, "Screen Size");
        UIUtils::drawUISprite(SLIDERX, SIZEY + 20, sliderImgs.getImage(BAR));

        const uint32_t offset = (5 - gScreenSize) * 18;
        UIUtils::drawUISprite(SLIDERX + 5 + offset, SIZEY + 20, sliderImgs.getImage(HANDLE));

        if (gbIsPlayingMap) {
            PrintBigFontCenter(160, QUITY, "Quit To Main");
        } else {
            PrintBigFontCenter(160, QUITY, "Exit Game");
        }
    }

    CelImages::releaseImages(rSLIDER);
    Video::endFrame(bPresent, bSaveFrameBuffer);
}
