#include "IntermissionScreen.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "GFX/CelImages.h"
#include "GFX/Video.h"
#include "UIUtils.h"

static constexpr int32_t KVALX      = 232;
static constexpr int32_t KVALY      = 70;
static constexpr int32_t IVALX      = 232;
static constexpr int32_t IVALY      = 100;
static constexpr int32_t SVALX      = 231;
static constexpr int32_t SVALY      = 130;
static constexpr int32_t INTERTIME  = TICKSPERSEC / 30;

// Intermission shape group
enum {
    KillShape,
    ItemsShape,
    SecretsShape
};

// Here are the names of each level
static constexpr const char* const MAP_NAMES[] = {
    "Hangar",
    "Nuclear Plant",
    "Toxin Refinery",
    "Command Control",
    "Phobos Lab",
    "Central Processing",
    "Computer Station",
    "Phobos Anomaly",
    "Deimos Anomaly",
    "Containment Area",
    "Refinery",
    "Deimos Lab",
    "Command Center",
    "Halls of the Damned",
    "Spawning Vats",
    "Tower of Babel",
    "Hell Keep",
    "Pandemonium",
    "House of Pain",
    "Unholy Cathedral",
    "Mt. Erebus",
    "Limbo",
    "Dis",
    "Military Base"
};

static constexpr const char* const FINISHED = "Finished";
static constexpr const char* const ENTERING = "Entering";

static uint32_t gKillPercent;        // Percent to attain
static uint32_t gItemPercent;
static uint32_t gSecretPercent;
static uint32_t gKillValue;         // Displayed percent value
static uint32_t gItemValue;
static uint32_t gSecretValue;
static uint32_t gINDelay;            // Delay before next inc
static uint32_t gBangCount;          // Delay for gunshot sound

//------------------------------------------------------------------------------------------------------------------------------------------
// Init the intermission data
//------------------------------------------------------------------------------------------------------------------------------------------
void IN_Start() noexcept {
    gINDelay = 0;
    gBangCount = 0;
    gKillValue = gItemValue = gSecretValue = 0;             // All values shown are zero
    gKillPercent = gItemPercent = gSecretPercent = 100;     // Init in case of divide by zero

    if (gTotalKillsInLevel > 0) {
        gKillPercent = (gPlayer.killcount * 100) / gTotalKillsInLevel;
    }

    if (gItemsFoundInLevel > 0) {
        gItemPercent = (gPlayer.itemcount * 100) / gItemsFoundInLevel;
    }

    if (gSecretsFoundInLevel > 0) {
        gSecretPercent = (gPlayer.secretcount * 100) / gSecretsFoundInLevel;
    }

    S_StartSong(Song_intermission);     // Begin the music
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Exit the intermission
//------------------------------------------------------------------------------------------------------------------------------------------
void IN_Stop() noexcept {
    S_StopSong();   // Kill the music
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Exit the intermission
//------------------------------------------------------------------------------------------------------------------------------------------
gameaction_e IN_Ticker() noexcept {    
    if (gTotalGameTicks < TICKSPERSEC / 2) {    // Initial wait before I begin
        return ga_nothing;                      // Do nothing
    }

    // Pressed a button?
    if (MENU_ACTION_ENDED(OK) || MENU_ACTION_ENDED(BACK)) {
        gKillValue = gKillPercent;          // Set to maximums
        gItemValue = gItemPercent;
        gSecretValue = gSecretPercent;
        return ga_died;                     // Exit after drawing
    }

    ++gINDelay;
    
    if (gINDelay >= INTERTIME) {
        bool bBang = false;
        gINDelay -= INTERTIME;

        if (gKillValue < gKillPercent) {    // Is it too high?
            ++gKillValue;
            bBang = true;
        }

        if (gItemValue < gItemPercent) {
            ++gItemValue;
            bBang = true;
        }

        if (gSecretValue < gSecretPercent) {
            ++gSecretValue;
            bBang = true;
        }

        if (bBang) {
            ++gBangCount;
            if (gBangCount % 4 == 0) {
                S_StartSound(nullptr, sfx_pistol);
            }
        }
    }

    return ga_nothing;  // Still here!
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Draw the intermission screen
//------------------------------------------------------------------------------------------------------------------------------------------
void IN_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    Video::debugClearScreen();
    UIUtils::drawUISprite(0, 0, rBACKGRNDBROWN);    // Load and draw the skulls

    const CelImageArray& intermisShapes = CelImages::loadImages(rINTERMIS, CelLoadFlagBits::MASKED);

    UIUtils::printBigFontCenter(160, 10, MAP_NAMES[gGameMap - 1]);   // Print the current map name
    UIUtils::printBigFontCenter(160, 34, FINISHED);                  // Print "Finished"

    if (gNextMap != 23) {
        UIUtils::printBigFontCenter(160, 162, ENTERING);
        UIUtils::printBigFontCenter(160, 182, MAP_NAMES[gNextMap - 1]);
    }

    UIUtils::drawUISprite(71, KVALY, intermisShapes.getImage(KillShape));   // Draw the shapes
    UIUtils::drawUISprite(65, IVALY, intermisShapes.getImage(ItemsShape));
    UIUtils::drawUISprite(27, SVALY, intermisShapes.getImage(SecretsShape));

    UIUtils::printNumber(KVALX, KVALY, gKillValue, UIUtils::PNFLAGS_PERCENT|UIUtils::PNFLAGS_RIGHT);   // Print the numbers
    UIUtils::printNumber(IVALX, IVALY, gItemValue, UIUtils::PNFLAGS_PERCENT|UIUtils::PNFLAGS_RIGHT);
    UIUtils::printNumber(SVALX, SVALY, gSecretValue, UIUtils::PNFLAGS_PERCENT|UIUtils::PNFLAGS_RIGHT);

    CelImages::releaseImages(rINTERMIS);
    UIUtils::drawPerformanceCounter(0, 0);

    Video::endFrame(bPresent, bSaveFrameBuffer);
}
