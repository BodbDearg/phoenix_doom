#include "Tick.h"

#include "Audio/Audio.h"
#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Mem.h"
#include "Base/Random.h"
#include "Burger.h"
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

struct thinker_t {
    thinker_t* next;
    thinker_t* prev;
    void (*function)(thinker_t*);
};

static uint32_t     gTimeMark1;         // Timer for ticks
static uint32_t     gTimeMark2;         // Timer for ticks
static uint32_t     gTimeMark4;         // Timer for ticks
static thinker_t    gThinkerCap;        // Both the head and tail of the thinker list
static bool         gRefreshDrawn;      // Used to refresh "Paused"

mobj_t  gMObjHead;
bool    gTick4;
bool    gTick2;
bool    gTick1;
bool    gGamePaused;

/**********************************

    Remove a thinker structure from the linked list and from
    memory.

**********************************/

static void RemoveMeThink(thinker_t* Current)
{
    thinker_t *Next;
    thinker_t *Prev;
    --Current;                  // Index to the REAL pointer
    Next = Current->next;
    Prev = Current->prev;
    Next->prev = Prev;          // Unlink it
    Prev->next = Next;
    MemFree(Current);           // Release the memory
}

/**********************************

    Init the mobj list and the thinker list
    I use a circular linked list with a mobjhead and thinkercap
    structure used only as an anchor point. These header structures
    are not used in actual gameplay, only for a referance point.
    If this is the first time through (!thinkercap.next) then just
    init the vars, else follow the memory chain and dispose of the
    memory.

**********************************/
void InitThinkers()
{
    ResetPlats();               // Reset the platforms
    ResetCeilings();            // Reset the ceilings
    
    if (gMObjHead.next) {    // Initialized before?
        mobj_t *m,*NextM;
        m=gMObjHead.next;
        while (m!=&gMObjHead) {     // Any player object
            NextM = m->next;        // Get the next record
            MemFree(m);             // Delete the object from the list
            m=NextM;
        }
    }
    
    if (gThinkerCap.next) {
        thinker_t *t,*NextT;
        t = gThinkerCap.next;
        while (t!=&gThinkerCap) {   // Is there a think struct here?
            NextT = t->next;
            MemFree(t);             // Delete it from memory
            t = NextT;
        }
    }
    
    gThinkerCap.prev = gThinkerCap.next  = &gThinkerCap;    // Loop around
    gMObjHead.next = gMObjHead.prev = &gMObjHead;           // Loop around
}

/**********************************

    Adds a new thinker at the end of the list, this way,
    I can get my code executed before the think execute routine
    finishes.

**********************************/
void* AddThinker(ThinkerFunc FuncProc, uint32_t MemSize)
{
    thinker_t *Prev;
    thinker_t *thinker;
    
    MemSize += sizeof(thinker_t);               // Add size for the thinker prestructure
    thinker = (thinker_t*) MemAlloc(MemSize);   // Get memory
    memset(thinker,0,MemSize);                  // Blank it out
    Prev = gThinkerCap.prev;                    // Get the last thinker in the list
    thinker->next = &gThinkerCap;               // Mark as last entry in list
    thinker->prev = Prev;                       // Set prev link to final entry
    thinker->function = FuncProc;
    Prev->next = thinker;                       // Next link to the new link
    gThinkerCap.prev = thinker;                 // Mark the reverse link
    return thinker+1;                           // Index AFTER the thinker structure
}

/**********************************

    Deallocation is lazy -- it will not actually be freed until its
    thinking turn comes up

**********************************/
void RemoveThinker(void* thinker)
{
    thinker = ((thinker_t *)thinker)-1;                 // Index to the true structure
    ((thinker_t*)thinker)->function = RemoveMeThink;    // Delete the structure on the next pass
}

/**********************************

    Modify a thinker's code function
    
**********************************/

void ChangeThinkCode(void* thinker, ThinkerFunc FuncProc)
{
    thinker = ((thinker_t *)thinker)-1;
    ((thinker_t *)thinker)->function = FuncProc;
}

/**********************************

    Execute all the think logic in the object list

**********************************/

void RunThinkers()
{
    thinker_t *currentthinker;
    thinker_t *NextThinker;

    currentthinker = gThinkerCap.next;       // Get the first entry
    while (currentthinker != &gThinkerCap) { // Looped back?
        // Get the next entry (In case of change or removal)
        NextThinker = currentthinker->next;
        if (currentthinker->function) {     // Is the function ptr ok?
            // Call the think logic
            // Note : It may be a call to a think remove routine!
            currentthinker->function(currentthinker+1);
        }
        currentthinker = NextThinker;   // Go to the next entry
    }
}

/**********************************

    Check the cheat keys... :)

**********************************/

static void CheckCheats()
{
    if ((gNewJoyPadButtons & PadStart) && !(gPlayers.AutomapFlags & AF_OPTIONSACTIVE)) {      // Pressed pause?
        if (gGamePaused || !(gJoyPadButtons&gPadUse)) {
            gGamePaused ^= 1;   // Toggle the pause flag

            if (gGamePaused) {
                audioPauseSound();
                audioPauseMusic();
            } else {
                audioResumeSound();
                audioResumeMusic();
            }
        }
    }
}

/**********************************

    Code that gets executed every game frame

**********************************/

uint32_t P_Ticker()
{
    player_t *pl;

    // wait for refresh to latch all needed data before
    // running the next tick

    gGameAction = ga_nothing;   // Game in progress
    gTick1 = false;             // Reset the flags
    gTick2 = false;
    gTick4 = false;

    gTimeMark1 += gElapsedTime;     // Timer for ticks
    gTimeMark2 += gElapsedTime;
    gTimeMark4 += gElapsedTime;

    if (gTimeMark1 >= TICKSPERSEC) {    // Now see if the time has passed...
        gTimeMark1 -= TICKSPERSEC;
        gTick1 = true;
    }

    if (gTimeMark2 >= TICKSPERSEC / 2) {
        gTimeMark2 -= TICKSPERSEC / 2;
        gTick2 = true;
    }

    if (gTimeMark4 >= TICKSPERSEC / 4) {
        gTimeMark4 -= TICKSPERSEC / 4;
        gTick4 = true;
    }

    CheckCheats();      // Handle pause and cheats

// Do option screen processing and control reading

    if (gGamePaused) {       // If in pause mode, then don't do any game logic
        return gGameAction;
    }

// Run player actions

    pl = &gPlayers;
    
    if (pl->playerstate == PST_REBORN) {    // Restart player?
        G_DoReborn();       // Poof!!
    }
    AM_Control(*pl);    // Handle automap controls
    O_Control(pl);      // Handle option controls
    P_PlayerThink(pl);  // Process player in the game
        
    if (!(gPlayers.AutomapFlags & AF_OPTIONSACTIVE)) {
        RunThinkers();      // Handle logic for doors, walls etc...
        P_RunMobjBase();    // Handle critter think logic
    }
    P_UpdateSpecials(); // Handle wall and floor animations
    ST_Ticker();        // Update status bar
    return gGameAction; // may have been set to ga_died, ga_completed,
                        // or ga_secretexit
}

//--------------------------------------------------------------------------------------------------
// Draw current display
//--------------------------------------------------------------------------------------------------
void P_Drawer() {
    if (gGamePaused && gRefreshDrawn) {
        DrawPlaque(rPAUSED);            // Draw 'Paused' plaque
        Video::present();
    } else if (gPlayers.AutomapFlags & AF_OPTIONSACTIVE) {
        Video::debugClear();
        Renderer::drawPlayerView();     // Render the 3D view
        ST_Drawer();                    // Draw the status bar
        O_Drawer();                     // Draw the console handler
        gRefreshDrawn = false;
    } else if (gPlayers.AutomapFlags & AF_ACTIVE) {
        Video::debugClear();
        AM_Drawer();                // Draw the automap
        ST_Drawer();                // Draw the status bar
        Video::present();           // Update and page flip
        gRefreshDrawn = true;
    } else {
        Video::debugClear();
        Renderer::drawPlayerView();     // Render the 3D view
        ST_Drawer();                    // Draw the status bar
        Video::present();               // Only allow debug clear if we are not going into pause mode
        gRefreshDrawn = true;
    }
}

//--------------------------------------------------------------------------------------------------
// Start a game
//--------------------------------------------------------------------------------------------------
void P_Start() {
    gTimeMark1 = 0;  // Init the static timers
    gTimeMark2 = 0;
    gTimeMark4 = 0;
    gPlayers.AutomapFlags &= AF_GODMODE;    // No automapping specials (but allow godmode)

    AM_Start();         // Start the automap system
    ST_Start();         // Init the status bar this level
    G_DoLoadLevel();    // Load a level into memory
    Random::init();     // Reset the random number generator

    S_StartSong(Song_e1m1 - 1 + gGameMap);
}

//--------------------------------------------------------------------------------------------------
// Shut down a game
//--------------------------------------------------------------------------------------------------
void P_Stop() {
    S_StopSong();
    ST_Stop();              // Release the status bar memory
    ReleaseMapMemory();     // Release all the map's memory
    PurgeLineSpecials();    // Release the memory for line specials
}
