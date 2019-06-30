#include "Tick.h"

#include "Audio/Audio.h"
#include "Automap_Main.h"
#include "Data.h"
#include "DoomRez.h"
#include "Game.h"
#include "MapObj.h"
#include "Mem.h"
#include "Player.h"
#include "Random.h"
#include "Render_Main.h"
#include "Sounds.h"
#include "Specials.h"
#include <cstring>

struct thinker_t {
    thinker_t* next;
    thinker_t* prev;
    void (*function)(thinker_t*);
};

static Word         TimeMark1;      // Timer for ticks
static Word         TimeMark2;      // Timer for ticks
static Word         TimeMark4;      // Timer for ticks
static thinker_t    thinkercap;     // Both the head and tail of the thinker list
static bool         refreshdrawn;   // Used to refresh "Paused"

mobj_t  mobjhead;
bool    Tick4;
bool    Tick2;
bool    Tick1;
bool    gamepaused;

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
    
    if (mobjhead.next) {    // Initialized before?
        mobj_t *m,*NextM;
        m=mobjhead.next;
        while (m!=&mobjhead) {      // Any player object
            NextM = m->next;        // Get the next record
            MemFree(m);             // Delete the object from the list
            m=NextM;
        }
    }
    
    if (thinkercap.next) {
        thinker_t *t,*NextT;
        t = thinkercap.next;
        while (t!=&thinkercap) {    // Is there a think struct here?
            NextT = t->next;
            MemFree(t);             // Delete it from memory
            t = NextT;
        }
    }
    
    thinkercap.prev = thinkercap.next  = &thinkercap;   // Loop around
    mobjhead.next = mobjhead.prev = &mobjhead;          // Loop around
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
    Prev = thinkercap.prev;                     // Get the last thinker in the list
    thinker->next = &thinkercap;                // Mark as last entry in list
    thinker->prev = Prev;                       // Set prev link to final entry
    thinker->function = FuncProc;
    Prev->next = thinker;                       // Next link to the new link
    thinkercap.prev = thinker;                  // Mark the reverse link
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

    currentthinker = thinkercap.next;       // Get the first entry
    while (currentthinker != &thinkercap) { // Looped back?
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
    if ((NewJoyPadButtons & PadStart) && !(players.AutomapFlags & AF_OPTIONSACTIVE)) {      // Pressed pause?
        if (gamepaused || !(JoyPadButtons&PadUse)) {
            gamepaused ^= 1;        // Toggle the pause flag

            if (gamepaused) {
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

    gameaction = ga_nothing;        // Game in progress
    Tick1 = false;          // Reset the flags
    Tick2 = false;
    Tick4 = false;

    TimeMark1+=gElapsedTime; // Timer for ticks
    TimeMark2+=gElapsedTime;
    TimeMark4+=gElapsedTime;

    if (TimeMark1>=TICKSPERSEC) {   // Now see if the time has passed...
        TimeMark1-=TICKSPERSEC;
        Tick1 = true;
    }
    if (TimeMark2>=(TICKSPERSEC/2)) {
        TimeMark2-=(TICKSPERSEC/2);
        Tick2 = true;
    }
    if (TimeMark4>=(TICKSPERSEC/4)) {
        TimeMark4-=(TICKSPERSEC/4);
        Tick4 = true;
    }

    CheckCheats();      // Handle pause and cheats

// Do option screen processing and control reading

    if (gamepaused) {       // If in pause mode, then don't do any game logic
        return gameaction;
    }

// Run player actions

    pl = &players;
    
    if (pl->playerstate == PST_REBORN) {    // Restart player?
        G_DoReborn();       // Poof!!
    }
    AM_Control(*pl);    // Handle automap controls
    O_Control(pl);      // Handle option controls
    P_PlayerThink(pl);  // Process player in the game
        
    if (!(players.AutomapFlags & AF_OPTIONSACTIVE)) {
        RunThinkers();      // Handle logic for doors, walls etc...
        P_RunMobjBase();    // Handle critter think logic
    }
    P_UpdateSpecials(); // Handle wall and floor animations
    ST_Ticker();        // Update status bar
    return gameaction;  // may have been set to ga_died, ga_completed,
                        // or ga_secretexit
}

//--------------------------------------------------------------------------------------------------
// Draw current display
//--------------------------------------------------------------------------------------------------
void P_Drawer() {
    bool bAllowDebugClear = (!gamepaused);

    if (gamepaused && refreshdrawn) {
        DrawPlaque(rPAUSED);                    // Draw 'Paused' plaque
        UpdateAndPageFlip(bAllowDebugClear);
    } else if (players.AutomapFlags & AF_OPTIONSACTIVE) {
        R_RenderPlayerView();                   // Render the 3D view
        ST_Drawer();                            // Draw the status bar
        O_Drawer();                             // Draw the console handler
        refreshdrawn = false;
    } else if (players.AutomapFlags & AF_ACTIVE) {
        AM_Drawer();                            // Draw the automap
        ST_Drawer();                            // Draw the status bar
        UpdateAndPageFlip(bAllowDebugClear);    // Update and page flip
        refreshdrawn = true;
    } else {
        R_RenderPlayerView();                   // Render the 3D view
        ST_Drawer();                            // Draw the status bar
        UpdateAndPageFlip(!bAllowDebugClear);   // Only allow debug clear if we are not going into pause mode
        refreshdrawn = true;
    }
}

//--------------------------------------------------------------------------------------------------
// Start a game
//--------------------------------------------------------------------------------------------------
void P_Start() {
    TimeMark1 = 0;  // Init the static timers
    TimeMark2 = 0;
    TimeMark4 = 0;
    players.AutomapFlags &= AF_GODMODE;     // No automapping specials (but allow godmode)

    AM_Start();         // Start the automap system
    ST_Start();         // Init the status bar this level
    G_DoLoadLevel();    // Load a level into memory
    Random::init();     // Reset the random number generator

    S_StartSong(Song_e1m1 - 1 + gamemap);
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
