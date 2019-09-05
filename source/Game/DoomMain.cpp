#include "DoomMain.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Input.h"
#include "Controls.h"
#include "Data.h"
#include "DoomDefines.h"
#include "DoomRez.h"
#include "Game.h"
#include "GFX/CelImages.h"
#include "GFX/Renderer.h"
#include "GFX/Video.h"
#include "Map/Setup.h"
#include "TickCounter.h"
#include "UI/IntroLogos.h"
#include "UI/IntroMovies.h"
#include "UI/Menu_Main.h"
#include "UI/Options_Main.h"
#include "WipeFx.h"
#include <thread>

//----------------------------------------------------------------------------------------------------------------------
// Grow a box if needed to encompass a point
//
// DC: TODO: Should this be elsewhere?
// Was here in the original source, but 'dmain' does not seem appropriate.
//----------------------------------------------------------------------------------------------------------------------
void AddToBox(Fixed* box, Fixed x, Fixed y) noexcept {
    if (x < box[BOXLEFT]) {             // Off the left side?
        box[BOXLEFT] = x;               // Increase the left
    } else if (x > box[BOXRIGHT]) {     // Off the right side?
        box[BOXRIGHT] = x;              // Increase the right
    }

    if (y < box[BOXBOTTOM]) {           // Off the top of the box?
        box[BOXBOTTOM] = y;             // Move the top
    } else if (y > box[BOXTOP]) {       // Off the bottom of the box?
        box[BOXTOP] = y;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Main loop processing for the game system.
// Each callback is optional, though should probably always have a ticker and drawer.
//----------------------------------------------------------------------------------------------------------------------
gameaction_e MiniLoop(
    const GameLoopStartFunc start,
    const GameLoopStopFunc stop,
    const GameLoopTickFunc ticker,
    const GameLoopDrawFunc drawer
) noexcept {
    // Do a wipe by default unless the start handler cancels it
    gDoWipe = true;

    // Initialize ticking
    gTotalGameTicks = 0;
    TickCounter::init();

    // Setup (cache graphics,etc)
    if (start) {
        start();
    }

    gameaction_e nextGameAction = ga_nothing;   // I am running
    gGameAction = ga_nothing;                   // Game is not in progress

    // Do a wipe if one is desired (start func can abort if desired)
    if (gDoWipe) {
        WipeFx::doWipe(drawer);
    }
    
    // Run the game loop until instructed to exit
    do {
        // See how many ticks are to be simulated, if none then sleep for a bit
        uint32_t ticksLeftToSimulate = TickCounter::update();

        if (ticksLeftToSimulate <= 0) {
            std::this_thread::yield();
            continue;
        }

        // Update input and controls and if a quit was requested then exit immediately
        Input::update();
        Controls::update();

        if (Input::isQuitRequested()) {
            nextGameAction = ga_quit;
            break;
        }

        // Simulate the required number of ticks
        if (ticker) {
            while ((ticksLeftToSimulate > 0) && (nextGameAction == ga_nothing)) {
                ++gTotalGameTicks;              // Add to the VBL count
                --ticksLeftToSimulate;
                nextGameAction = ticker();      // Process the keypad commands

                if (ticksLeftToSimulate > 0) {
                    Input::consumeEvents();     // Don't allow any more keypress events if we do more than 1 tick
                    Controls::update();         // Update controls based on that for the next tick
                }
            }
        }

        // Did I end the level? If so then exit after drawing
        if (gGameAction == ga_warped) {
            nextGameAction = ga_warped;
        }

        // Sync up with the refresh - draw the screen.
        // Also save the framebuffer for each game loop transition, so we can do a wipe if needed.
        if (drawer) {
            const bool bPresent = true;
            const bool bSaveFrameBuffer = (nextGameAction != ga_nothing);
            drawer(bPresent, bSaveFrameBuffer);
        }

    } while (nextGameAction == ga_nothing);     // Is the loop finished?

    // Cleanup, release resources etc.
    if (stop) {
        stop();     
    }

    S_Clear();                  // Kill sounds
    gPlayer.mo = nullptr;       // For net consistancy checks
    TickCounter::shutdown();
    return nextGameAction;      // Return the abort code from the loop
}

//----------------------------------------------------------------------------------------------------------------------
// If key's A, B or C was pressed or 8 seconds of demo was shown then abort the demo.
//----------------------------------------------------------------------------------------------------------------------
static gameaction_e TIC_Abortable() noexcept {
    if (gTotalGameTicks >= (8 * TICKSPERSEC)) {         // Time up?
        return ga_died;                                 // Go on to next demo
    }

    const bool bShouldExit = (MENU_ACTION(OK) || MENU_ACTION(BACK));
    return (bShouldExit) ? ga_exitdemo : ga_nothing;
}

//----------------------------------------------------------------------------------------------------------------------
// Load the title picture into memory
//----------------------------------------------------------------------------------------------------------------------
static bool gOnlyOnce;

static void START_Title() noexcept {
    if (!gOnlyOnce) {
        gOnlyOnce = true;
        gDoWipe = false;    // On power up, don't wipe the screen
    }

    S_StartSong(Song_intro);
}

//----------------------------------------------------------------------------------------------------------------------
// Release the memory for the title picture
//----------------------------------------------------------------------------------------------------------------------
static void STOP_Title() noexcept {
    // Nothing to do...
}

//----------------------------------------------------------------------------------------------------------------------
// Draws the title page
//----------------------------------------------------------------------------------------------------------------------
static void DRAW_Title(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    Video::debugClearScreen();
    Renderer::drawUISprite(0, 0, rTITLE);           // Draw the doom logo
    Video::endFrame(bPresent, bSaveFrameBuffer);
}

//----------------------------------------------------------------------------------------------------------------------
// Ticker code for the credits page
//----------------------------------------------------------------------------------------------------------------------
static gameaction_e TIC_Credits() noexcept {
    if (gTotalGameTicks >= (10 * TICKSPERSEC)) {    // Time up?
        return ga_died;                             // Go on to next demo
    }

    const bool bShouldExit = (MENU_ACTION(OK) || MENU_ACTION(BACK));
    return (bShouldExit) ? ga_exitdemo : ga_nothing;
}

//----------------------------------------------------------------------------------------------------------------------
// Draw the credits pages
//----------------------------------------------------------------------------------------------------------------------
static void DRAW_Credits(const bool bPresent, const bool bSaveFrameBuffer, const uint32_t creditsPageResourceNum) noexcept {
    Video::debugClearScreen();
    Renderer::drawUISprite(0, 0, creditsPageResourceNum);
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

//----------------------------------------------------------------------------------------------------------------------
// Execute the main menu
//----------------------------------------------------------------------------------------------------------------------
static void RunMenu() {
    if (Input::isQuitRequested())
        return;

    if (MiniLoop(M_Start, M_Stop, M_Ticker, M_Drawer) == ga_completed) {
        S_StopSong();
        G_InitNew(gStartSkill, gStartMap);      // Init the new game
        G_RunGame();                            // Play the game
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Run the title page, returns 'true' if credits should show after this call
//----------------------------------------------------------------------------------------------------------------------
static bool RunTitle() noexcept {
    if (Input::isQuitRequested())
        return false;
    
    // Run the main menu if the user exited out of this screen
    if (MiniLoop(START_Title, STOP_Title, TIC_Abortable, DRAW_Title) == ga_exitdemo) {
        RunMenu();
        return false;
    } else {
        return true;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Show the game credit pages
//----------------------------------------------------------------------------------------------------------------------
static void RunCredits() noexcept {
    if (Input::isQuitRequested())
        return;

    // Show ID credits, Art Data Interactive credits and then Logicware credits in that order.
    // If the user requests to exit this sequence then go to the main menu.
    if (MiniLoop(nullptr, nullptr, TIC_Credits, DRAW_IdCredits) == ga_exitdemo) {
        RunMenu();
        return;
    }

    if (MiniLoop(nullptr, nullptr, TIC_Credits, DRAW_AdiCredits) == ga_exitdemo) {
        RunMenu();
        return;
    }

    if (MiniLoop(nullptr, nullptr, TIC_Credits, DRAW_LogicwareCredits) == ga_exitdemo) {
        RunMenu();
        return;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Main entry point for DOOM!!!!
//----------------------------------------------------------------------------------------------------------------------
void D_DoomMain() noexcept {
    gpBigNumFont = &CelImages::loadImages(rBIGNUMB, CelLoadFlagBits::MASKED);   // Cache the large numeric font (Needed always)

    Renderer::init();   // Init refresh system
    P_Init();           // Init main code
    O_Init();           // Init options menu

    IntroLogos::run();
    IntroMovies::run();

    while (!Input::isQuitRequested()) {
        const bool bDoCreditsNext = RunTitle();

        if (bDoCreditsNext) {
            RunCredits();
        }
    }
}
