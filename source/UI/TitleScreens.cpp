#include "TitleScreens.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Input.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "Game/Game.h"
#include "GFX/Video.h"
#include "MainMenu.h"
#include "UIUtils.h"
#include "WipeFx.h"

BEGIN_NAMESPACE(TitleScreens)

//------------------------------------------------------------------------------------------------------------------------------------------
// If key's A, B or C was pressed or 8 seconds of demo was shown then abort the demo.
//------------------------------------------------------------------------------------------------------------------------------------------
static gameaction_e TIC_Abortable() noexcept {
    if (gTotalGameTicks >= (8 * TICKSPERSEC)) {         // Time up?
        return ga_died;                                 // Go on to next demo
    }

    const bool bShouldExit = (MENU_ACTION(OK) || MENU_ACTION(BACK));
    return (bShouldExit) ? ga_exitdemo : ga_nothing;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Load the title picture into memory
//------------------------------------------------------------------------------------------------------------------------------------------
static bool gOnlyOnce;

static void START_Title() noexcept {
    if (!gOnlyOnce) {
        gOnlyOnce = true;
        gbDoWipe = false;   // On power up, don't wipe the screen
    }

    S_StartSong(Song_intro);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Release the memory for the title picture
//------------------------------------------------------------------------------------------------------------------------------------------
static void STOP_Title() noexcept {
    // Nothing to do...
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Draws the title page
//------------------------------------------------------------------------------------------------------------------------------------------
static void DRAW_Title(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    Video::debugClearScreen();
    UIUtils::drawUISprite(0, 0, rTITLE);           // Draw the doom logo
    UIUtils::drawPerformanceCounter(0, 0);
    Video::endFrame(bPresent, bSaveFrameBuffer);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Ticker code for the credits page
//------------------------------------------------------------------------------------------------------------------------------------------
static gameaction_e TIC_Credits() noexcept {
    if (gTotalGameTicks >= (10 * TICKSPERSEC)) {    // Time up?
        return ga_died;                             // Go on to next demo
    }

    const bool bShouldExit = (MENU_ACTION(OK) || MENU_ACTION(BACK));
    return (bShouldExit) ? ga_exitdemo : ga_nothing;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Draw the credits pages
//------------------------------------------------------------------------------------------------------------------------------------------
static void DRAW_Credits(const bool bPresent, const bool bSaveFrameBuffer, const uint32_t creditsPageResourceNum) noexcept {
    Video::debugClearScreen();
    UIUtils::drawUISprite(0, 0, creditsPageResourceNum);
    UIUtils::drawPerformanceCounter(0, 0);
    Video::endFrame(bPresent, bSaveFrameBuffer);
}

static void DRAW_IdCredits(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    DRAW_Credits(bPresent, bSaveFrameBuffer, rIDCREDITS);
}

static void DRAW_AdiCredits(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    DRAW_Credits(bPresent, bSaveFrameBuffer, rCREDITS);
}

static void DRAW_LogicwareCredits(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    DRAW_Credits(bPresent, bSaveFrameBuffer, rLOGCREDITS);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Execute the main menu
//------------------------------------------------------------------------------------------------------------------------------------------
static void RunMenu() {
    if (Input::isQuitRequested())
        return;

    if (RunGameLoop(M_Start, M_Stop, M_Ticker, M_Drawer) == ga_completed) {
        S_StopSong();
        G_InitNew(gStartSkill, gStartMap);      // Init the new game
        G_RunGame();                            // Play the game
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Run the title page, returns 'true' if credits should show after this call
//------------------------------------------------------------------------------------------------------------------------------------------
bool runTitleScreen() noexcept {
    if (Input::isQuitRequested())
        return false;
    
    // Run the main menu if the user exited out of this screen
    if (RunGameLoop(START_Title, STOP_Title, TIC_Abortable, DRAW_Title) == ga_exitdemo) {
        RunMenu();
        return false;
    } else {
        return true;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Show the game credit pages
//------------------------------------------------------------------------------------------------------------------------------------------
void runCreditScreens() noexcept {
    if (Input::isQuitRequested())
        return;

    // Show ID credits, Art Data Interactive credits and then Logicware credits in that order.
    // If the user requests to exit this sequence then go to the main menu.
    if (RunGameLoop(nullptr, nullptr, TIC_Credits, DRAW_IdCredits) == ga_exitdemo) {
        RunMenu();
        return;
    }

    if (RunGameLoop(nullptr, nullptr, TIC_Credits, DRAW_AdiCredits) == ga_exitdemo) {
        RunMenu();
        return;
    }

    if (RunGameLoop(nullptr, nullptr, TIC_Credits, DRAW_LogicwareCredits) == ga_exitdemo) {
        RunMenu();
        return;
    }
}

END_NAMESPACE(TitleScreens)
