#include "Tick.h"

#include "Audio/Audio.h"
#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Mem.h"
#include "Base/Random.h"
#include "Controls.h"
#include "Data.h"
#include "DoomDefines.h"
#include "DoomRez.h"
#include "Game.h"
#include "GFX/Renderer.h"
#include "GFX/Video.h"
#include "Map/Ceiling.h"
#include "Map/Platforms.h"
#include "Map/Setup.h"
#include "Map/Specials.h"
#include "Things/Base.h"
#include "Things/MapObj.h"
#include "Things/User.h"
#include "ThreeDO.h"
#include "UI/Automap_Main.h"
#include "UI/Options_Main.h"
#include "UI/StatusBar_Main.h"
#include <cstring>
#include <SDL.h>

// Define to profile rendering (outputs to console)
#define PROFILE_RENDER 1

struct thinker_t {
    thinker_t*      next;
    thinker_t*      prev;
    ThinkerFunc     function;
};

static uint32_t     gTimeMark1;         // Timer for ticks
static uint32_t     gTimeMark2;         // Timer for ticks
static uint32_t     gTimeMark4;         // Timer for ticks
static thinker_t    gThinkerCap;        // Both the head and tail of the thinker list
static bool         gbRefreshDrawn;     // Used to refresh "Paused"

mobj_t  gMObjHead;
bool    gbTick4;
bool    gbTick2;
bool    gbTick1;
bool    gbGamePaused;

//----------------------------------------------------------------------------------------------------------------------
// Remove a thinker structure from the linked list and from memory.
//----------------------------------------------------------------------------------------------------------------------
static void RemoveMeThink(thinker_t& thinker) noexcept {
    thinker_t* const pActualThinker = &thinker - 1;     // Index to the REAL pointer
    thinker_t* const pNext = pActualThinker->next;
    thinker_t* const pPrev = pActualThinker->prev;
    ASSERT(pNext);
    ASSERT(pPrev);
    pNext->prev = pPrev;            // Unlink it
    pPrev->next = pNext;
    MemFree(pActualThinker);        // Release the memory
}

//----------------------------------------------------------------------------------------------------------------------
// Init the mobj list and the thinker list.
// I use a circular linked list with a mobjhead and thinkercap structure used only as an anchor point.
// These header structures are not used in actual gameplay, only for a referance point.
// If this is the first time through (!thinkercap.next) then just init the vars, else follow the memory chain and
// dispose of the memory.
//----------------------------------------------------------------------------------------------------------------------
void InitThinkers() noexcept {
    ResetPlats();           // Reset the platforms
    ResetCeilings();        // Reset the ceilings

    if (gMObjHead.next) {   // Initialized before?
        mobj_t* pMObj = gMObjHead.next;
        while (pMObj != &gMObjHead) {                   // Any player object
            mobj_t* const pNextMObj = pMObj->next;      // Get the next record
            MemFree(pMObj);                             // Delete the object from the list
            pMObj = pNextMObj;
        }
    }

    if (gThinkerCap.next) {
        thinker_t* pThinker = gThinkerCap.next;
        while (pThinker != &gThinkerCap) {                      // Is there a think struct here?
            thinker_t* const pNextThinker = pThinker->next;
            MemFree(pThinker);                                  // Delete it from memory
            pThinker = pNextThinker;
        }
    }

    gThinkerCap.prev = gThinkerCap.next  = &gThinkerCap;    // Loop around
    gMObjHead.next = gMObjHead.prev = &gMObjHead;           // Loop around
}

//----------------------------------------------------------------------------------------------------------------------
// Adds a new thinker at the end of the list.
// This way, I can get my code executed before the think execute routine finishes.
//----------------------------------------------------------------------------------------------------------------------
void* AddThinker(const ThinkerFunc funcProc, const uint32_t memSize) noexcept {
    const uint32_t allocSize = memSize + sizeof(thinker_t);         // Add size for the thinker prestructure
    thinker_t* const pThinker = (thinker_t*) MemAlloc(allocSize);   // Get memory
    memset(pThinker, 0, allocSize);                                 // Blank it out

    thinker_t* const pPrevLastThinker = gThinkerCap.prev;   // Get the last thinker in the list
    ASSERT(pPrevLastThinker);

    pThinker->next = &gThinkerCap;                          // Mark as last entry in list
    pThinker->prev = pPrevLastThinker;                      // Set prev link to final entry
    pThinker->function = funcProc;
    pPrevLastThinker->next = pThinker;                      // Next link to the new link

    gThinkerCap.prev = pThinker;    // Mark the reverse link
    return pThinker + 1;            // Index AFTER the thinker structure
}

//----------------------------------------------------------------------------------------------------------------------
// Deallocation is lazy - it will not actually be freed until its thinking turn comes up
//----------------------------------------------------------------------------------------------------------------------
void RemoveThinker(void* const pThinker) noexcept {
    thinker_t* const pActualThinker = ((thinker_t*) pThinker) - 1;      // Index to the true structure
    pActualThinker->function = RemoveMeThink;                           // Delete the structure on the next pass
}

//----------------------------------------------------------------------------------------------------------------------
// Modify a thinker's code function
//----------------------------------------------------------------------------------------------------------------------
void ChangeThinkCode(void* const pThinker, const ThinkerFunc funcProc) noexcept {
    thinker_t* const pActualThinker = ((thinker_t*) pThinker) - 1;
    pActualThinker->function = funcProc;
}

//----------------------------------------------------------------------------------------------------------------------
// Execute all the think logic in the object list
//----------------------------------------------------------------------------------------------------------------------
void RunThinkers() noexcept {
    thinker_t* pCurThinker = gThinkerCap.next;  // Get the first entry

    while (pCurThinker != &gThinkerCap) {                       // Looped back?        
        thinker_t* const pNextThinker = pCurThinker->next;      // Get the next entry (In case of change or removal)

        // Call the think logic if present. Note: it may be a call to a think remove routine!
        if (pCurThinker->function) {            
            pCurThinker->function(pCurThinker[1]);
        }

        pCurThinker = pNextThinker;     // Go to the next entry
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Check for the pause button
//----------------------------------------------------------------------------------------------------------------------
static void checkForPauseButton() noexcept {
    if (gPlayer.isOptionsMenuActive())  // Can't pause on options screen!
        return;
    
    if (GAME_ACTION_ENDED(PAUSE)) {
        gbGamePaused = (!gbGamePaused);

        if (gbGamePaused) {
            Audio::pauseAllSounds();
            Audio::pauseMusic();
        } else {
            Audio::resumeAllSounds();
            Audio::resumeMusic();
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Code that gets executed every game frame
//----------------------------------------------------------------------------------------------------------------------
gameaction_e P_Ticker() noexcept {
    // Wait for refresh to latch all needed data before running the next tick
    gGameAction = ga_nothing;   // Game in progress
    gbTick1 = false;            // Reset the flags
    gbTick2 = false;
    gbTick4 = false;

    ++gTimeMark1;   // Timer for ticks
    ++gTimeMark2;
    ++gTimeMark4;

    if (gTimeMark1 >= TICKSPERSEC) {    // Now see if the time has passed...
        gTimeMark1 -= TICKSPERSEC;
        gbTick1 = true;
    }

    if (gTimeMark2 >= TICKSPERSEC / 2) {
        gTimeMark2 -= TICKSPERSEC / 2;
        gbTick2 = true;
    }

    if (gTimeMark4 >= TICKSPERSEC / 4) {
        gTimeMark4 -= TICKSPERSEC / 4;
        gbTick4 = true;
    }

    checkForPauseButton();

    // If in pause mode, then don't do any game logic
    if (gbGamePaused) {
        return gGameAction;
    }

    // Run player actions
    player_t& player = gPlayer;

    if (player.playerstate == PST_REBORN) {     // Restart player?
        G_DoReborn();                           // Poof!!
    }

    AM_Control(player);         // Handle automap controls
    O_Control(&player);         // Handle option controls
    P_PlayerThink(player);      // Process player in the game

    if (!gPlayer.isOptionsMenuActive()) {
        RunThinkers();      // Handle logic for doors, walls etc...
        P_RunMobjBase();    // Handle critter think logic
    }

    P_UpdateSpecials();     // Handle wall and floor animations
    ST_Ticker();            // Update status bar
    return gGameAction;     // May have been set to 'ga_died', 'ga_completed', or 'ga_secretexit'
}

//----------------------------------------------------------------------------------------------------------------------
// Draw current display
//----------------------------------------------------------------------------------------------------------------------
void P_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    #if PROFILE_RENDER
        const uint64_t clocksPerSecond = SDL_GetPerformanceFrequency();
        const uint64_t drawStartClock = SDL_GetPerformanceCounter();
    #endif

    if (gbGamePaused && gbRefreshDrawn) {
        DrawPlaque(rPAUSED);            // Draw 'Paused' plaque
        Video::endFrame(bPresent, bSaveFrameBuffer);
    } else if (gPlayer.isOptionsMenuActive()) {
        Video::debugClearScreen();
        Renderer::drawPlayerView();                     // Render the 3D view
        ST_Drawer();                                    // Draw the status bar
        O_Drawer(bPresent, bSaveFrameBuffer);           // Draw the console handler
        gbRefreshDrawn = false;
    } else if (gPlayer.isAutomapActive()) {
        Video::debugClearScreen();
        AM_Drawer();                                    // Draw the automap
        ST_Drawer();                                    // Draw the status bar
        Video::endFrame(bPresent, bSaveFrameBuffer);
        gbRefreshDrawn = true;
    } else {
        Video::debugClearScreen();
        Renderer::drawPlayerView();                     // Render the 3D view
        ST_Drawer();                                    // Draw the status bar
        Video::endFrame(bPresent, bSaveFrameBuffer);
        gbRefreshDrawn = true;
    }

    #if PROFILE_RENDER
        const uint64_t drawEndClock = SDL_GetPerformanceCounter();
        const uint64_t drawClockCount = drawEndClock - drawStartClock;
        const double drawTime = (double) drawClockCount / (double) clocksPerSecond;
        std::printf("Draw time: %f seconds, %zu clock ticks\n", drawTime, (size_t) drawClockCount);
    #endif
}

//----------------------------------------------------------------------------------------------------------------------
// Start a game
//----------------------------------------------------------------------------------------------------------------------
void P_Start() noexcept {
    gTimeMark1 = 0;  // Init the static timers
    gTimeMark2 = 0;
    gTimeMark4 = 0;
    gPlayer.AutomapFlags &= (AF_GODMODE|AF_NOCLIP);     // No automapping specials (but preserve godmode and noclip cheats)

    AM_Start();                     // Start the automap system
    ST_Start();                     // Init the status bar this level
    G_DoLoadLevel();                // Load a level into memory
    Random::init();                 // Reset the random number generator
    PlayerCalcHeight(gPlayer);      // Required for the view to be at the right height for the screen wipe

    // Reapply the noclip cheat to the player map object if that cheat was enabled in the previous level
    if ((gPlayer.AutomapFlags & AF_NOCLIP) != 0) {
        gPlayer.mo->flags |= (MF_NOCLIP|MF_SOLID);
    }

    S_StartSong(Song_e1m1 - 1 + gGameMap);
}

//----------------------------------------------------------------------------------------------------------------------
// Shut down a game
//----------------------------------------------------------------------------------------------------------------------
void P_Stop() noexcept {
    S_StopSong();
    ST_Stop();              // Release the status bar memory
    ReleaseMapMemory();     // Release all the map's memory
    PurgeLineSpecials();    // Release the memory for line specials
}
