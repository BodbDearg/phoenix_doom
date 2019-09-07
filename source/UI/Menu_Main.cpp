#include "Menu_Main.h"

#include "Base/Input.h"
#include "Game/Config.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "GFX/CelImages.h"
#include "GFX/Renderer.h"
#include "GFX/Video.h"
#include "Intermission_Main.h"
#include "Options_Main.h"
#include "ThreeDO.h"

static constexpr int32_t CURSORX        = 50;       // X coord of skull cursor
static constexpr int32_t AREAY          = 66;
static constexpr int32_t DIFFICULTYY    = 116;
static constexpr int32_t OPTIONSY       = 166;

enum menu_t {
    level,
    difficulty,
    options,
    NUMMENUITEMS
};

// Enum to index to the shapes
enum {
    DIFFSHAPE1,
    DIFFSHAPE2,
    DIFFSHAPE3,
    DIFFSHAPE4,
    DIFFSHAPE5,
    AREASHAPE,
    DIFFSHAPE
};

static uint32_t gCursorFrame;       // Skull animation frame
static uint32_t gCursorCount;       // Time mark to animate the skull
static uint32_t gCursorPos;         // Y position of the skull
static uint32_t gMoveCount;         // Time mark to move the skull
static uint32_t gPlayerMap;         // Map requested
static uint32_t gPlayerSkill;       // Requested skill
static uint32_t gSleepMark;         // Time from last access

static uint32_t gCursorYs[NUMMENUITEMS] = {
    AREAY - 2,
    DIFFICULTYY - 2,
    OPTIONSY - 2
};

static uint32_t gOptionActive;

//--------------------------------------------------------------------------------------------------
// Init memory needed for the main game menu
//--------------------------------------------------------------------------------------------------
void M_Start() noexcept {    
    gCursorCount = 0;               // Init the animation timer
    gCursorFrame = 0;               // Init the animation frame
    gCursorPos = 0;                 // Topmost y position
    gPlayerSkill = gStartSkill;     // Init the skill level
    gPlayerMap = gStartMap;         // Init the starting map
    gSleepMark = gTotalGameTicks;
    gOptionActive = false;          // Option screen on
}

//--------------------------------------------------------------------------------------------------
// Release memory used by the main menu
//--------------------------------------------------------------------------------------------------
void M_Stop() noexcept {
    WritePrefsFile();               // Save the current prefs
}

//--------------------------------------------------------------------------------------------------
// Execute every tick
//--------------------------------------------------------------------------------------------------
gameaction_e M_Ticker() noexcept {
    // Exit menu if 'OK' pressed on the right option and enough time has elapsed
    if ((gTotalGameTicks > TICKSPERSEC / 2) &&
        MENU_ACTION_ENDED(OK) &&
        (gCursorPos != options)
    ) {
        gStartMap = gPlayerMap;                 // Set map number
        gStartSkill = (skill_e) gPlayerSkill;   // Set skill level
        return ga_completed;                    // Done with menu
    }

    // If buttons are held down then reset the timer
    if (Input::areAnyKeysOrButtonsPressed()) {
        gSleepMark = gTotalGameTicks;
    }

    if (gOptionActive) {
        O_Control(nullptr);

        if (MENU_ACTION_ENDED(BACK) || MENU_ACTION_ENDED(OK)) {
            gOptionActive = false;
        }

        return ga_nothing;
    }

    // Backing out of the main menu or timing out? If so then exit now
    if (MENU_ACTION_ENDED(BACK) || (gTotalGameTicks - gSleepMark >= TICKSPERSEC * 15)) {
        return ga_died;
    }

    // Animate skull
    ++gCursorCount;                         // Add time
    if (gCursorCount >= TICKSPERSEC / 4) {  // Time to toggle the shape?
        gCursorFrame ^= 1;
        gCursorCount = 0;                   // Reset the count
    }

    // Switch to options menu?
    if (gCursorPos == options && MENU_ACTION_ENDED(OK)) {
        O_Init();   // Reset option menu positions
        gOptionActive = true;
    }

    // Check for up/down/left/right movement
    if (!Input::areAnyKeysOrButtonsPressed()) {
        // Move immediately on next press if nothing is pressed
        gMoveCount = TICKSPERSEC;
    } else {
        ++gMoveCount;   // Time unit

        if ((gMoveCount >= TICKSPERSEC / 4) ||                      // Allow slow
            (gCursorPos == level && gMoveCount >= TICKSPERSEC / 5)  // Fast?
        ) {
            // Reset the timer
            gMoveCount = 0;

            // Gather up/down and left/right menu movements
            int menuMoveX = 0;
            int menuMoveY = 0;
            Controls::gatherAnalogAndDigitalMenuMovements(menuMoveX, menuMoveY);

            // Up and down menu movement
            if (menuMoveY >= 1) {
                ++gCursorPos;
                if (gCursorPos >= NUMMENUITEMS) {
                    gCursorPos = 0;
                }
            }
            else if (menuMoveY <= -1) {
                if (gCursorPos == 0) {
                    gCursorPos = NUMMENUITEMS;
                }
                --gCursorPos;
            }

            // Other menu input
            switch (gCursorPos) {
                // Select level to start with
                case level: {
                    if (menuMoveX >= 1) {
                        if (gPlayerMap < gMaxLevel) {
                            ++gPlayerMap;
                        }
                    }
                    else if (menuMoveX <= -1) {
                        if (gPlayerMap != 1) {
                            --gPlayerMap;
                        }
                    }
                }   break;

                // Select game difficulty
                case difficulty: {
                    if (menuMoveX >= 1) {
                        if (gPlayerSkill < sk_nightmare) {
                            ++gPlayerSkill;
                        }
                    }
                    else if (menuMoveX <= -1) {
                        if (gPlayerSkill > 0) {
                            --gPlayerSkill;
                        }
                    }
                }   break;
            }
        }
    }

    return ga_nothing;  // Don't quit!
}

//--------------------------------------------------------------------------------------------------
// Draw the main menu
//--------------------------------------------------------------------------------------------------
void M_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    Video::debugClearScreen();
    Renderer::drawUISprite(0, 0, rMAINDOOM);

    if (gOptionActive) {
        O_Drawer(bPresent, bSaveFrameBuffer);
    }
    else {
        const CelImageArray& shapes = CelImages::loadImages(rMAINMENU, CelLoadFlagBits::MASKED);    // Load shape group

        // Draw new skull
        const CelImageArray& skullImgs = CelImages::loadImages(rSKULLS, CelLoadFlagBits::MASKED);
        Renderer::drawUISprite(CURSORX, gCursorYs[gCursorPos], skullImgs.getImage(gCursorFrame));
        CelImages::releaseImages(rSKULLS);

        // Draw start level information
        PrintBigFont(CURSORX + 24, AREAY, "Level");
        PrintNumber(CURSORX + 40, AREAY + 20, gPlayerMap, 0);

        // Draw difficulty information
        Renderer::drawUISprite(CURSORX + 24, DIFFICULTYY, shapes.getImage(DIFFSHAPE));
        Renderer::drawUISprite(CURSORX + 40, DIFFICULTYY + 20, shapes.getImage(gPlayerSkill));

        // Draw the options screen
        PrintBigFont(CURSORX + 24, OPTIONSY, "Options Menu");
        CelImages::releaseImages(rMAINMENU);
        Video::endFrame(bPresent, bSaveFrameBuffer);
    }
}
