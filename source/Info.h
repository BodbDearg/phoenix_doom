#pragma once

#include "States.h"
#include "Doom.h"

struct pspdef_t;

// Think functions for a player sprite and map object
typedef void (*PspActionFunc)(player_t*, pspdef_t*);
typedef void (*MObjActionFunc)(mobj_t*);

// An actor's state
struct state_t {    
    uint32_t SpriteFrame;   // Which sprite to display?
    uint32_t Time;          // Time before next action

    // Logic to execute for next state
    union {
        const MObjActionFunc  mobjAction;   // Used for map object actions
        const PspActionFunc   pspAction;    // Used for player sprite actions
    };
    
    struct state_t* nextstate;  // Index to state table for next state

    // Constructor for player sprite state
    constexpr inline state_t(
        uint32_t SpriteFrame,
        uint32_t Time,
        PspActionFunc pspAction,
        state_t* nextstate
    ) noexcept
        : SpriteFrame(SpriteFrame)
        , Time(Time)
        , pspAction(pspAction)
        , nextstate(nextstate)
    {
    }

    // Constructor for map object state
    constexpr inline state_t(
        uint32_t SpriteFrame,
        uint32_t Time,
        MObjActionFunc mobjAction,
        state_t* nextstate
    ) noexcept
        : SpriteFrame(SpriteFrame)
        , Time(Time)
        , mobjAction(mobjAction)
        , nextstate(nextstate)
    {
    }
};

// Describe an actor's basic variables
struct mobjinfo_t {
    state_t*    spawnstate;     // State number to be spawned
    state_t*    seestate;       // First action state
    state_t*    painstate;      // State when in pain
    state_t*    meleestate;     // State when attacking
    state_t*    missilestate;   // State for missile weapon
    state_t*    deathstate;     // State for normal death
    state_t*    xdeathstate;    // State for gruesome death
    Fixed       Radius;         // Radius for collision checking
    Fixed       Height;         // Height for collision checking
    uint32_t    doomednum;      // Number in doom ed
    uint32_t    spawnhealth;    // Hit points at spawning
    uint32_t    painchance;     // % chance of pain when hit
    uint32_t    mass;           // Mass for impact recoil
    uint32_t    flags;          // Generic state flags
    uint8_t     Speed;          // Rate of speed for normal chase or walk
    uint8_t     reactiontime;   // Time before first action
    uint8_t     damage;         // Damage done for attack
    uint8_t     seesound;       // Sound effect after first action
    uint8_t     attacksound;    // Sound when attacking
    uint8_t     painsound;      // Pain sound
    uint8_t     deathsound;     // Sound for normal death
    uint8_t     activesound;    // Sound to play at random times for mood
};

extern state_t      states[NUMSTATES];
extern mobjinfo_t   mobjinfo[NUMMOBJTYPES];
