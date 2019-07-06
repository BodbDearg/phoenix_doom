#include "DoomMain.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Burger.h"
#include "Data.h"
#include "DoomDefines.h"
#include "DoomRez.h"
#include "Game.h"
#include "GFX/Renderer.h"
#include "GFX/Video.h"
#include "Map/Setup.h"
#include "Resources.h"
#include "ThreeDO.h"
#include "UI/Menu_Main.h"
#include "UI/Options_Main.h"

// FIXME: DC: TEMP - REMOVE
#include <SDL.h>
#include <ctime>

//----------------------------------------------------------------------------------------------------------------------
// Grow a box if needed to encompass a point
//
// DC: TODO: Should this be elsewhere?
// Was here in the original source, but 'dmain' does not seem appropriate.
//----------------------------------------------------------------------------------------------------------------------
void AddToBox(Fixed* box, Fixed x, Fixed y) {
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
// Convert a joypad response into a network joypad record.
// This is to compensate the fact that different computers have different hot keys for motion control.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t LocalToNet(uint32_t cmd) {
    uint32_t NewCmd = 0;

    if (cmd & gPadSpeed) {      // Was speed true?
        NewCmd = PadA;          // Save it
    }

    if (cmd & gPadAttack) {     // Was attack true?
        NewCmd |= PadB;         // Save it
    }

    if (cmd & gPadUse) {        // Was use true?
        NewCmd |= PadC;         // Save it
    }

    return (cmd & ~(PadA|PadB|PadC)) | NewCmd;  // Return the network compatible response
}

//----------------------------------------------------------------------------------------------------------------------
// Convert a network response into a local joypad record.
// This is to compensate the fact that different computers have different hot keys for motion control.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t NetToLocal(uint32_t cmd) {
    uint32_t NewCmd = 0;

    if (cmd & PadA) {
        NewCmd = gPadSpeed;     // Set the speed bit
    }

    if (cmd & PadB) {
        NewCmd |= gPadAttack;   // Set the attack bit
    }

    if (cmd & PadC) {
        NewCmd |= gPadUse;      // Set the use bit
    }

    return (cmd & ~(PadA|PadB|PadC)) | NewCmd;  // Return the localized joypad setting
}

//----------------------------------------------------------------------------------------------------------------------
// Read a joypad command byte from the demo data
//----------------------------------------------------------------------------------------------------------------------
static uint32_t GetDemoCmd() {
    const uint32_t cmd = gDemoDataPtr[0];       // Get a joypad byte from the demo stream
    ++gDemoDataPtr;                             // Inc the state
    return NetToLocal(cmd);                     // Convert the network command to local
}

//----------------------------------------------------------------------------------------------------------------------
// Main loop processing for the game system
//----------------------------------------------------------------------------------------------------------------------
uint32_t MiniLoop(
    void (*start)(),
    void (*stop)(),
    uint32_t (*ticker)(),
    void (*drawer)()
)
{
    // Setup (cache graphics,etc)
    gDoWipe = true;
    start();                        // Prepare the background task (Load data etc.)
    uint32_t exit = 0;              // I am running
    gGameAction = ga_nothing;       // Game is not in progress
    gTotalGameTicks = 0;            // No vbls processed during game
    gElapsedTime = 0;               // No time has elapsed yet

    // Init the joypad states
    gJoyPadButtons = gPrevJoyPadButtons = gNewJoyPadButtons = 0;

    do {        
        // FIXME: DC: Put this somewhere better
        SDL_PumpEvents();

        // FIXME: DC: TEMP
        static clock_t lastClock;
        clock_t curClock = clock();

        if ((curClock - lastClock) / (double) CLOCKS_PER_SEC > 1.0 / (float) TICKSPERSEC) {
            gElapsedTime = 1;
            lastClock = curClock;
        }
        else {
            gElapsedTime = 0;
            continue;
        }        

        // Run the tic immediately
        gTotalGameTicks += gElapsedTime;    // Add to the VBL count
        exit = ticker();                    // Process the keypad commands

        // Adaptive timing based on previous frame
        if (gDemoPlayback || gDemoRecording) {
            gElapsedTime = 4;                       // Force 15 FPS
        } else {
            gElapsedTime = (uint32_t) gLastTics;    // Get the true time count
            if (gElapsedTime >= 9) {                // Too slow?
                gElapsedTime = 8;                   // Make 7.5 fps as my mark
            }
        }

        // Get buttons for next tic
        gPrevJoyPadButtons = gJoyPadButtons;        // Pass through the latest keypad info
        uint32_t buttons = ReadJoyButtons(0);       // Read the controller
        gJoyPadButtons = buttons;                   // Save it
        
        if (gDemoPlayback) {                            // Playing back a demo?
            if (buttons & (PadA|PadB|PadC|PadD) ) {     // Abort?
                exit = ga_exitdemo;                     // Exit the demo
                break;
            }

            // Get a joypad from demo data
            gJoyPadButtons = buttons = GetDemoCmd();
        }

        gNewJoyPadButtons = (buttons ^ gPrevJoyPadButtons) & buttons;   // Get the joypad downs...

        if (gDemoRecording) {                           // Am I recording a demo?
            gDemoDataPtr[0] = LocalToNet(buttons);      // Save the current joypad data
            ++gDemoDataPtr;
        }

        // Am I recording a demo?
        if ((gDemoRecording || gDemoPlayback) && (buttons & PadStart) ) {
            exit = ga_completed;    // End the game right now!
        }

        if (gGameAction == ga_warped) {     // Did I end the level?
            exit = ga_warped;               // Exit after drawing
            break;                          // Exit
        }

        // Sync up with the refresh - draw the screen
        drawer();

    } while (!exit);    // Is the loop finished?

    stop();             // Release resources
    S_Clear();          // Kill sounds
    gPlayers.mo = 0;    // For net consistancy checks
    return exit;        // Return the abort code from ticker
}

//----------------------------------------------------------------------------------------------------------------------
// If key's A, B or C was pressed or 8 seconds of demo was shown then abort the demo.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t TIC_Abortable() {
    if (gTotalGameTicks >= (8 * TICKSPERSEC)) {         // Time up?
        return ga_died;                                 // Go on to next demo
    }

    if (gNewJoyPadButtons&(PadA|PadB|PadC|PadD)) {      // Pressed A B or C?
        return ga_exitdemo;                             // Exit the demo right now!
    }

    return ga_nothing;  // Continue the demo
}

//----------------------------------------------------------------------------------------------------------------------
// Load the title picture into memory
//----------------------------------------------------------------------------------------------------------------------
static bool gOnlyOnce;

static void START_Title() {
    if (!gOnlyOnce) {
        gOnlyOnce = true;
        gDoWipe = false;    // On power up, don't wipe the screen
    }

    S_StartSong(Song_intro);
}

//----------------------------------------------------------------------------------------------------------------------
// Release the memory for the title picture
//----------------------------------------------------------------------------------------------------------------------
static void STOP_Title() {
    // Nothing to do...
}

//----------------------------------------------------------------------------------------------------------------------
// Draws the title page
//----------------------------------------------------------------------------------------------------------------------
static void DRAW_Title() {
    Video::debugClear();
    DrawRezShape(0, 0, rTITLE);     // Draw the doom logo
    Video::present();
}

//----------------------------------------------------------------------------------------------------------------------
// Load resources for the credits page
//----------------------------------------------------------------------------------------------------------------------
static uint32_t gCreditRezNum;

static void START_Credits() {
    gCreditRezNum = rIDCREDITS;
}

//----------------------------------------------------------------------------------------------------------------------
// Release memory for credits
//----------------------------------------------------------------------------------------------------------------------
static void STOP_Credits() {
}

//----------------------------------------------------------------------------------------------------------------------
// Ticker code for the credits page
//----------------------------------------------------------------------------------------------------------------------
static uint32_t TIC_Credits() {
    if (gTotalGameTicks >= (30 * TICKSPERSEC)) {    // Time up?
        return ga_died;                             // Go on to next demo
    }

    if (gNewJoyPadButtons & (PadA|PadB|PadC|PadD)) {    // Pressed A,B or C?
        return ga_exitdemo;                             // Abort the demo
    }

    return ga_nothing;  // Don't stop!
}

//----------------------------------------------------------------------------------------------------------------------
// Draw the credits page
//----------------------------------------------------------------------------------------------------------------------
static void DRAW_Credits() {
    Video::debugClear();

    switch (gCreditRezNum) {
        case rIDCREDITS:
            if (gTotalGameTicks >= ( 10 * TICKSPERSEC)) {
                gCreditRezNum = rCREDITS;
                gDoWipe = true;
            }
            break;
        
        case rCREDITS:
            if (gTotalGameTicks >= (20 * TICKSPERSEC)) {
                gCreditRezNum = rLOGCREDITS;
                gDoWipe = true;
            }
    }

    DrawRezShape(0, 0, gCreditRezNum);  // Draw the credits
    Video::present();                   // Page flip
}

//----------------------------------------------------------------------------------------------------------------------
// Execute the main menu
//----------------------------------------------------------------------------------------------------------------------
static void RunMenu() {
    if (MiniLoop(M_Start, M_Stop, M_Ticker, M_Drawer) == ga_completed) {    // Process the menu
        S_StopSong();
        G_InitNew(gStartSkill, gStartMap);      // Init the new game
        G_RunGame();                            // Play the game
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Run the title page
//----------------------------------------------------------------------------------------------------------------------
static void RunTitle() {
    if (MiniLoop(START_Title, STOP_Title, TIC_Abortable, DRAW_Title) == ga_exitdemo) {
        RunMenu();  // Process the main menu
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Show the game credits
//----------------------------------------------------------------------------------------------------------------------
static void RunCredits() {
    if (MiniLoop(START_Credits, STOP_Credits, TIC_Credits, DRAW_Credits) == ga_exitdemo) {  // Did you quit?
        RunMenu();  // Process the main menu
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Run the game demo
//----------------------------------------------------------------------------------------------------------------------
static void RunDemo(uint32_t demoname) {
    // DC: This was disabled in the original 3DO source.
    // The 3DO version of the game did not ship with demos?
    #if 0
        Word *demo;
        Word exit;
        demo = (Word *)LoadAResource(demoname);     // Load the demo
        exit = G_PlayDemoPtr(demo);                 // Play the demo
        ReleaseAResource(demoname);                 // Release the demo data

        if (exit == ga_exitdemo) {      // Quit?
            RunMenu();                  // Show the main menu
        }
    #endif
}

//----------------------------------------------------------------------------------------------------------------------
// Main entry point for DOOM!!!!
//----------------------------------------------------------------------------------------------------------------------
void D_DoomMain() {
    gBigNumFont = loadResourceData(rBIGNUMB);   // Cache the large numeric font (Needed always)

    Renderer::init();   // Init refresh system
    P_Init();           // Init main code
    O_Init();           // Init controls

    for (;;) {
        RunTitle();         // Show the title page
        RunDemo(rDEMO1);    // Run the first demo
        RunCredits();       // Show the credits page
        RunDemo(rDEMO2);    // Run the second demo
    }
}
