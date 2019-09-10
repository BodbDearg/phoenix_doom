#include "DoomMain.h"

#include "Base/Input.h"
#include "Controls.h"
#include "Data.h"
#include "DoomRez.h"
#include "GFX/CelImages.h"
#include "GFX/Renderer.h"
#include "Map/Setup.h"
#include "TickCounter.h"
#include "UI/IntroLogos.h"
#include "UI/IntroMovies.h"
#include "UI/OptionsMenu.h"
#include "UI/TitleScreens.h"
#include "WipeFx.h"
#include <thread>

//----------------------------------------------------------------------------------------------------------------------
// Main loop processing for the game system.
// Each callback is optional, though should probably always have a ticker and drawer.
//----------------------------------------------------------------------------------------------------------------------
gameaction_e RunGameLoop(
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

    gPlayer.mo = nullptr;       // For net consistancy checks
    TickCounter::shutdown();
    return nextGameAction;      // Return the abort code from the loop
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
        const bool bDoCreditsNext = TitleScreens::runTitleScreen();

        if (bDoCreditsNext) {
            TitleScreens::runCreditScreens();
        }
    }
}
