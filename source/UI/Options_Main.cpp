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

static constexpr uint32_t CURSORX       = 45;       // X coord for skulls
static constexpr uint32_t SLIDERX       = 106;      // X coord for slider bars
static constexpr uint32_t SLIDESTEP     = 6;        // Adjustment for volume to screen coord
static constexpr uint32_t JOYPADX       = 90;       // X coord for joypad text
static constexpr uint32_t SFXVOLY       = 60;       // Y coord for SFX volume control
static constexpr uint32_t MUSICVOLY     = 120;      // Y coord for Music volume control
static constexpr uint32_t JOYPADY       = 40;       // Y coord for joypad control
static constexpr uint32_t SIZEY         = 140;      // Y coord for screen size control

// Action buttons can be set to PadA, PadB, or PadC
enum {     
    SFU,
    SUF,
    FSU,
    FUS,
    USF,
    UFS,
    NUMCONTROLOPTIONS    // Control option count
};

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

static uint32_t gCursorFrame;       // Skull animation frame
static uint32_t gCursorCount;       // Time mark to animate the skull
static uint32_t gCursorPos;         // Y position of the skull
static uint32_t gMoveCount;         // Time mark to move the skull

//----------------------------------------------------------------------------------------------------------------------
// Init the button settings from the control type
//----------------------------------------------------------------------------------------------------------------------
static void SetButtonsFromControltype() noexcept {
    const uint32_t* const pTable = &CONFIGURATION[gControlType][0];  // Init table

    gPadSpeed = pTable[0];          // Init the joypad settings
    gPadAttack = pTable[1];
    gPadUse = pTable[2];
    Renderer::initMathTables();     // Handle the math tables
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
    const uint32_t buttons = gJoyPadButtons;

    if (gNewJoyPadButtons & PadX) {      // Toggled the option screen?
        if (pPlayer) {
            pPlayer->AutomapFlags ^= AF_OPTIONSACTIVE;                  // Toggle the flag
            if ((pPlayer->AutomapFlags & AF_OPTIONSACTIVE) == 0) {      // Shut down?
                SetButtonsFromControltype();                            // Set the memory
                WritePrefsFile();                                       // Save new settings to NVRAM
            }
        } else {
            SetButtonsFromControltype();        // Set the memory
            WritePrefsFile();                   // Save new settings to NVRAM
        }
    }

    if (pPlayer) {
        if ((pPlayer->AutomapFlags & AF_OPTIONSACTIVE) == 0) {      // Can I execute?
            return;                                                 // Exit NOW!
        }
    }

    // Clear buttons so game player isn't moving around, but leave option status alone
    gJoyPadButtons = buttons & PadX;

    // Animate skull
    ++gCursorCount;
    if (gCursorCount >= TICKSPERSEC / 4) {      // Time up?
        gCursorFrame ^= 1;                      // Toggle the frame
        gCursorCount = 0;                       // Reset the timer
    }

    // Check for movement
    if ((buttons & (PadUp|PadDown|PadLeft|PadRight)) == 0) {
        gMoveCount = TICKSPERSEC;   // Move immediately on next press
    } else {
        ++gMoveCount;
        if ((gMoveCount >= TICKSPERSEC / 3) || (    // Allow slow
                (gCursorPos < controls) && 
                (gMoveCount >= TICKSPERSEC / 5)     // Fast?
            )
        ) {
            gMoveCount = 0;      // Reset timer

            // Try to move the cursor up or down...
            if ((buttons & PadDown) != 0) {
                ++gCursorPos;
                if (gCursorPos >= NUMMENUITEMS) {
                    gCursorPos = 0;
                }
            }

            if ((buttons & PadUp) != 0) {
                if (gCursorPos <= 0) {
                    gCursorPos = NUMMENUITEMS;
                }
                --gCursorPos;
            }

            switch (gCursorPos) {   // Adjust the control
                // Joypad?
                case controls: {
                    if ((buttons & PadRight) != 0) {
                        if (gControlType < NUMCONTROLOPTIONS - 1) {
                            ++gControlType;
                        }
                    }

                    if ((buttons & PadLeft) != 0) {
                        if (gControlType > 0) {
                            --gControlType;
                        }
                    }
                }   break;

                // Sound volume?
                case soundvol: {
                    if ((buttons & PadRight) != 0) {
                        const uint32_t soundVolume = audioGetSoundVolume();
                        if (soundVolume < MAX_AUDIO_VOLUME) {
                            audioSetSoundVolume(soundVolume + 1);
                            S_StartSound(0, sfx_pistol);
                        }
                    }

                    if ((buttons & PadLeft) != 0) {
                        const uint32_t soundVolume = audioGetSoundVolume();
                        if (soundVolume > 0) {
                            audioSetSoundVolume(soundVolume - 1);
                            S_StartSound(0, sfx_pistol);
                        }
                    }
                }   break;

                // Music volume?
                case musicvol: {
                    if ((buttons & PadRight) != 0) {
                        const uint32_t musicVolume = audioGetMusicVolume();
                        if (musicVolume < MAX_AUDIO_VOLUME) {
                            audioSetMusicVolume(musicVolume + 1);
                        }
                    }

                    if ((buttons & PadLeft) != 0) {
                        const uint32_t musicVolume = audioGetMusicVolume();
                        if (musicVolume > 0) {
                            audioSetMusicVolume(musicVolume - 1);
                        }
                    }
                }   break;

                // Screen size
                case size: {
                    if ((buttons & PadLeft) != 0)  {
                        if (gScreenSize < 6 - 1) {
                            ++gScreenSize;
                            if (pPlayer) {
                                Renderer::initMathTables();     // Handle the math tables
                            }
                        }
                    }

                    if ((buttons & PadRight) != 0) {
                        if (gScreenSize > 0) {
                            --gScreenSize;
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
    Video::endFrame(bPresent, bSaveFrameBuffer);
}
