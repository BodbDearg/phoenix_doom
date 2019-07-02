#pragma once

#include "Game/Doom.h"

struct mobjinfo_t;
struct player_t;
struct state_t;
struct subsector_t;

// Struct defining a thing or object in the map as it is defined in the level file
struct mapthing_t {
    Fixed       x;              // X,Y position (Signed)
    Fixed       y;
    angle_t     angle;          // Angle facing
    uint32_t    type;           // Object type
    uint32_t    ThingFlags;     // mapthing flags
};

// Flags for a map thing in the level file
static constexpr uint32_t MTF_EASY          = 0x1;
static constexpr uint32_t MTF_NORMAL        = 0x2;
static constexpr uint32_t MTF_HARD          = 0x4;
static constexpr uint32_t MTF_AMBUSH        = 0x8;
static constexpr uint32_t MTF_DEATHMATCH    = 0x10;

// Struct defining a runtime map object (thing)
struct mobj_t { 
    mobj_t*     prev;       // Linked list entries
    mobj_t*     next;
    Fixed       x;          // Location in 3Space
    Fixed       y;
    Fixed       z;

    // Info for drawing
    mobj_t*     snext;      // links in sector (if needed)
    mobj_t*     sprev;
    angle_t     angle;      // Angle of view

    // Interaction info
    mobj_t*         bnext;          // Links in blocks (if needed)
    mobj_t*         bprev;
    subsector_t*    subsector;      // Subsector currently standing on
    Fixed           floorz;         // Closest together of contacted secs
    Fixed           ceilingz;
    Fixed           radius;         // For movement checking
    Fixed           height;
    Fixed           momx;           // Momentums
    Fixed           momy;
    Fixed           momz;

    mobjinfo_t*     InfoPtr;        // Pointer to mobj info record
    uint32_t        tics;           // Time before next state
    state_t*        state;          // Pointer to current state record (Can't be NULL!)
    uint32_t        flags;          // State flags for object
    uint32_t        MObjHealth;     // Object's health
    uint32_t        movedir;        // 0-7
    uint32_t        movecount;      // When 0, select a new dir
    mobj_t*         target;         // Thing being chased/attacked (or NULL); also the originator for missiles.
    uint32_t        reactiontime;   // If non 0, don't attack yet; used by player to freeze a bit after teleporting.
    uint32_t        threshold;      // If > 0, the target will be chased no matter what (even if shot)
    player_t*       player;         // Only valid if type == MT_PLAYER
};

// Flags which can be used for map objects
static constexpr uint32_t MF_SPECIAL        = 0x1;          // Call P_SpecialThing when touched
static constexpr uint32_t MF_SOLID          = 0x2;
static constexpr uint32_t MF_SHOOTABLE      = 0x4;
static constexpr uint32_t MF_NOSECTOR       = 0x8;          // Don't use the sector links (invisible but touchable)
static constexpr uint32_t MF_NOBLOCKMAP     = 0x10;         // Don't use the BlockLinkPtr (inert but displayable)
static constexpr uint32_t MF_AMBUSH         = 0x20;         // Only attack when seen
static constexpr uint32_t MF_JUSTHIT        = 0x40;         // Try to attack right back
static constexpr uint32_t MF_JUSTATTACKED   = 0x80;         // Take at least one step before attacking
static constexpr uint32_t MF_SPAWNCEILING   = 0x100;        // Hang from ceiling instead of floor
static constexpr uint32_t MF_NOGRAVITY      = 0x200;        // Don't apply gravity every tic
static constexpr uint32_t MF_DROPOFF        = 0x400;        // Allow jumps from high places
static constexpr uint32_t MF_PICKUP         = 0x800;        // For players to pick up items
static constexpr uint32_t MF_NOCLIP         = 0x1000;       // Player cheat
static constexpr uint32_t MF_SLIDE          = 0x2000;       // Keep info about sliding along walls
static constexpr uint32_t MF_FLOAT          = 0x4000;       // Allow moves to any height, no gravity
static constexpr uint32_t MF_TELEPORT       = 0x8000;       // Don't cross lines or look at heights
static constexpr uint32_t MF_MISSILE        = 0x10000;      // Don't hit same species, explode on block
static constexpr uint32_t MF_DROPPED        = 0x20000;      // Dropped by a demon, not level spawned
static constexpr uint32_t MF_SHADOW         = 0x40000;      // Use fuzzy draw (shadow demons / invis)
static constexpr uint32_t MF_NOBLOOD        = 0x80000;      // Don't bleed when shot (use puff)
static constexpr uint32_t MF_CORPSE         = 0x100000;     // Don't stop moving halfway off a step
static constexpr uint32_t MF_INFLOAT        = 0x200000;     // Floating to a height for a move, don't auto float to target's height
static constexpr uint32_t MF_COUNTKILL      = 0x400000;     // Count towards intermission kill total
static constexpr uint32_t MF_COUNTITEM      = 0x800000;     // Count towards intermission item total
static constexpr uint32_t MF_SKULLFLY       = 0x1000000;    // Skull in flight
static constexpr uint32_t MF_NOTDMATCH      = 0x2000000;    // Don't spawn in death match (key cards)
static constexpr uint32_t MF_SEETARGET      = 0x4000000;    // Is target visible?

void P_RemoveMobj(mobj_t* th);
uint32_t SetMObjState(mobj_t* mobj, state_t* StatePtr);
void Sub1RandomTick(mobj_t* mobj);
void ExplodeMissile(mobj_t* mo);
mobj_t* SpawnMObj(Fixed x, Fixed y, Fixed z, mobjinfo_t* InfoPtr);
void P_SpawnPlayer(mapthing_t* mthing);
void SpawnMapThing(mapthing_t* mthing);
void P_SpawnPuff(Fixed x, Fixed y, Fixed z);
void P_SpawnBlood(Fixed x, Fixed y, Fixed z, uint32_t damage);
void P_SpawnMissile(mobj_t* source, mobj_t* dest, mobjinfo_t* InfoPtr);
void SpawnPlayerMissile(mobj_t* source, mobjinfo_t* InfoPtr);
