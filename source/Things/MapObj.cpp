#include "MapObj.h"

#include "Audio/Sound.h"
#include "Base/Mem.h"
#include "Base/Random.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "Game/Game.h"
#include "Game/Tick.h"
#include "Info.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Map/Setup.h"
#include <cstring>

// Bit field for item spawning based on level
static constexpr uint32_t LEVEL_BIT_MASKS[5] = {
    MTF_EASY,
    MTF_EASY,
    MTF_NORMAL,
    MTF_HARD,
    MTF_HARD
};

//----------------------------------------------------------------------------------------------------------------------
// Remove a monster object from memory
//----------------------------------------------------------------------------------------------------------------------
void P_RemoveMobj(mobj_t& mobj) noexcept {
    // Unlink from sector and block lists
    UnsetThingPosition(mobj);

    // Unlink from mobj list and release mem
    mobj.next->prev = mobj.prev;
    mobj.prev->next = mobj.next;
    MemFree(&mobj);
}

//----------------------------------------------------------------------------------------------------------------------
// Returns true if the mobj is still present
//----------------------------------------------------------------------------------------------------------------------
uint32_t SetMObjState(mobj_t& mobj, const state_t* const StatePtr) noexcept {
    if (!StatePtr) {                // Shut down state?
        P_RemoveMobj(mobj);         // Remove the object
        return false;               // Object is shut down
    }

    mobj.state = StatePtr;              // Save the state index
    mobj.tics = StatePtr->Time;         // Reset the tic count

    if (StatePtr->mobjAction) {         // Call action functions when the state is set
        StatePtr->mobjAction(mobj);     // Call the procedure
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// To make some death spawning a little more random, subtract a little random time value to mix up the deaths.
//----------------------------------------------------------------------------------------------------------------------
void Sub1RandomTick(mobj_t& mobj) noexcept {
    const uint32_t delay = Random::nextU32(3);      // Get the random adjustment
    if (mobj.tics >= delay) {                       // Too large?
        mobj.tics -= delay;                         // Set the new time
    } else {
        mobj.tics = 0;                              // Force a minimum
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Detonate a missile
//----------------------------------------------------------------------------------------------------------------------
void ExplodeMissile(mobj_t& mo) noexcept {
    mo.momx = mo.momy = mo.momz = 0;                // Stop forward momentum
    SetMObjState(mo, mo.InfoPtr->deathstate);       // Enter explosion state
    Sub1RandomTick(mo);
    mo.flags &= ~MF_MISSILE;                        // It's not a missile anymore
    S_StartSound(&mo.x, mo.InfoPtr->deathsound);    // Play the sound if any
}

//----------------------------------------------------------------------------------------------------------------------
// Spawn a misc object
//----------------------------------------------------------------------------------------------------------------------
mobj_t& SpawnMObj(const Fixed x, const Fixed y, const Fixed z, const mobjinfo_t& info) noexcept {
    mobj_t& mObj = AllocObj<mobj_t>();          // Alloc and init object memory
    memset(&mObj, 0, sizeof(mobj_t));

    mObj.InfoPtr = &info;                       // Save the type pointer
    mObj.x = x;                                 // Set the x (In Fixed pixels)
    mObj.y = y;
    mObj.radius = info.Radius;                  // Radius of object
    mObj.height = info.Height;                  // Height of object
    mObj.flags = info.flags;                    // Misc flags
    mObj.MObjHealth = info.spawnhealth;         // Health at start
    mObj.reactiontime = info.reactiontime;      // Initial reaction time

    const state_t* pSpawnState = info.spawnstate;       // Get the initial state
    if (!pSpawnState) {                                 // If null, then it's dormant
        pSpawnState = &gStates[S_NULL];                 // Since I am creating this, I MUST have a valid state
    }

    mObj.state = pSpawnState;           // Save the state pointer. Do not set the state with SetMObjState, because action routines can't be called yet!
    mObj.tics = pSpawnState->Time;      // Init the tics

    // Set subsector and/or block links
    SetThingPosition(mObj);                             // Attach to floor
    sector_t* const pSector = mObj.subsector->sector;
    mObj.floorz = pSector->floorheight;                 // Get the objects top clearance
    mObj.ceilingz = pSector->ceilingheight;             // And bottom clearance

    if (z == ONFLOORZ) {
        mObj.z = mObj.floorz;                               // Set the floor z
    } else if (z == ONCEILINGZ) {
        mObj.z = mObj.ceilingz - mObj.InfoPtr->Height;      // Adjust from ceiling
    } else {
        mObj.z = z;                                         // Use the raw z
    }

    // Link into the END of the mobj list
    gMObjHead.prev->next = &mObj;               // Set the new forward link
    mObj.next = &gMObjHead;                     // Set my next link
    mObj.prev = gMObjHead.prev;                 // Set my previous link
    gMObjHead.prev = &mObj;                     // Link myself in
    return mObj;                                // Return the new object pointer
}

//----------------------------------------------------------------------------------------------------------------------
// Called when a player is spawned on the level
// Most of the player structure stays unchanged between levels
//----------------------------------------------------------------------------------------------------------------------
void P_SpawnPlayer(const mapthing_t& mthing) noexcept {
    {
        const uint32_t i = mthing.type - 1;     // Which player entry?
        if (i > 0) {                            // In the game?
            return;                             // not playing
        }
    }

    player_t& player = gPlayer;                 // Get the player #
    if (player.playerstate == PST_REBORN) {     // Did you die?
        G_PlayerReborn();                       // Reset the player's variables
    }

    mobj_t& mObj = SpawnMObj(mthing.x, mthing.y, ONFLOORZ, gMObjInfo[MT_PLAYER]);

    mObj.angle = mthing.angle;          // Get the facing angle
    mObj.player = &player;              // Save the player's pointer
    mObj.MObjHealth = player.health;    // Init the health

    player.mo = &mObj;                  // Save the mobj into the player record
    player.playerstate = PST_LIVE;      // I LIVE!!!
    player.refire = false;              // Not autofiring (Yet)
    player.message = nullptr;           // No message passed
    player.damagecount = 0;             // No damage taken
    player.bonuscount = 0;              // No bonus awarded
    player.extralight = 0;              // No light goggles
    player.fixedcolormap = 0;           // Normal color map
    player.viewheight = VIEWHEIGHT;     // Normal height
    SetupPSprites(player);              // Setup gun psprite
}

//----------------------------------------------------------------------------------------------------------------------
// Create an item for the map
//----------------------------------------------------------------------------------------------------------------------
void SpawnMapThing(const mapthing_t& mthing) noexcept {
    // Count deathmatch start positions
    const uint32_t type = mthing.type;
    if (type == 11) {                                   // Starting positions
        if (gpDeathmatch < &gDeathmatchStarts[10]) {    // Full?
            gpDeathmatch[0] = mthing;
        }
        ++gpDeathmatch;     // I have a starting position
        return;             // Exit now
    }

    // Check for players specially (Object 0-4)
    if (type < 5) {
        // save spots for respawning in network games
        if (type < 1 + 1) {
            gPlayerStarts = mthing;     // Save the start spot
            P_SpawnPlayer(mthing);      // Create the player
        }
        return;                         // Did spawning
    }

    // Check for apropriate skill level and game mode
    if ((mthing.ThingFlags & MTF_DEATHMATCH) != 0) {    // Must be in deathmatch?
        return;                                         // Don't spawn, requires deathmatch
    }

    if ((mthing.ThingFlags & LEVEL_BIT_MASKS[gGameSkill]) == 0) {   // Can I spawn this?
        return;                                                     // Can't spawn: wrong skill level
    }

    // Find which type to spawn
    uint32_t i = 0;
    const mobjinfo_t* pInfo = gMObjInfo;    // Get pointer to table

    do {
        if (pInfo->doomednum == type) {       // Match?
            // Spawn it
            Fixed z = ONFLOORZ;                     // Most of the time...
            if (pInfo->flags & MF_SPAWNCEILING) {   // Attach to ceiling or floor
                z = ONCEILINGZ;
            }
            mobj_t& mObj = SpawnMObj(mthing.x, mthing.y, z, *pInfo);    // Create the object

            if (mObj.tics) {    // Randomize the initial tic count
                if (mObj.tics != -1) {
                    mObj.tics = Random::nextU32(mObj.tics) + 1;
                }
            }

            if ((mObj.flags & MF_COUNTKILL) != 0) {     // Must be killed?
                ++gTotalKillsInLevel;
            }

            if ((mObj.flags & MF_COUNTITEM) != 0) {     // Must be collected?
                ++gItemsFoundInLevel;
            }

            mObj.angle = mthing.angle;                      // Round the angle
            if ((mthing.ThingFlags & MTF_AMBUSH) != 0) {    // Will it wait in ambush?
                mObj.flags |= MF_AMBUSH;
            }
            return;
        }
        ++pInfo;    // Next entry

    } while (++i < NUMMOBJTYPES);
}

//----------------------------------------------------------------------------------------------------------------------
// Spawn a puff of smoke on the wall
//----------------------------------------------------------------------------------------------------------------------
void P_SpawnPuff(const Fixed x, const Fixed y, const Fixed z) noexcept {
    const Fixed zActual = z + ((255 - Random::nextU32(511)) << 10);     // Randomize the z
    mobj_t& mObj = SpawnMObj(x, y, zActual, gMObjInfo[MT_PUFF]);        // Create a puff
    mObj.momz = FRACUNIT;                                               // Allow it to move up per frame
    Sub1RandomTick(mObj);

    // Don't make punches spark on the wall
    if (gAttackRange == MELEERANGE) {
        SetMObjState(mObj, gStates[S_PUFF3]);       // Reset to the third state
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Blow up a body REAL GOOD!
//----------------------------------------------------------------------------------------------------------------------
void P_SpawnBlood(const Fixed x, const Fixed y, const Fixed z, const uint32_t damage) noexcept {
    const Fixed zActual = z + ((255 - Random::nextU32(511)) << 10);     // Move a little for the Z
    mobj_t& mObj = SpawnMObj(x, y, z, gMObjInfo[MT_BLOOD]);             // Create the blood (Hamburger)
    mObj.momz = FRACUNIT * 2;                                           // Allow some ascending motion
    Sub1RandomTick(mObj);

    if (damage < 13 && damage >= 9) {
        SetMObjState(mObj, gStates[S_BLOOD2]);      // Smaller mess
    } else if (damage < 9) {
        SetMObjState(mObj, gStates[S_BLOOD3]);      // Mild mess
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Moves the missile forward a bit and possibly explodes it right there
//----------------------------------------------------------------------------------------------------------------------
static void P_CheckMissileSpawn(mobj_t& mObj) noexcept {
    mObj.x += (mObj.momx >> 1);     // Adjust x and y based on momentum.
    mObj.y += (mObj.momy >> 1);     // Move a little forward so an angle can be computed if it immediately explodes
    mObj.z += (mObj.momz >> 1);

    if (!P_TryMove(mObj, mObj.x, mObj.y)) {     // Impact?
        ExplodeMissile(mObj);                   // Boom!!!
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Launch a missile from a monster to a player or other monster.
//----------------------------------------------------------------------------------------------------------------------
void P_SpawnMissile(mobj_t& source, mobj_t& dest, const mobjinfo_t& info) noexcept {
    // Create the missile object (32 pixels off the ground)
    mobj_t& th = SpawnMObj(source.x, source.y, source.z + (32 * FRACUNIT), info);

    S_StartSound(&source.x, info.seesound);                             // Play the launch sound
    th.target = &source;                                                // Who launched it?
    angle_t an = PointToAngle(source.x, source.y, dest.x, dest.y);      // Angle of travel

    if ((dest.flags & MF_SHADOW) != 0) {            // Hard to see, miss on purpose!
        an += (255 - Random::nextU32(511)) << 20;
    }

    th.angle = an;
    an >>= ANGLETOFINESHIFT;                        // Convert to internal table record
    uint32_t speed = info.Speed;                    // Get the speed
    th.momx = (Fixed) speed * gFineCosine[an];
    th.momy = (Fixed) speed * gFineSine[an];

    uint32_t dist = GetApproxDistance(dest.x - source.x, dest.y - source.y);
    dist = dist / (Fixed)(speed << FRACBITS);   // Convert to frac
    if (dist <= 0) {                            // Prevent divide by zero
        dist = 1;
    }

    th.momz = (dest.z - source.z) / (Fixed) dist;   // Set the z momentum
    P_CheckMissileSpawn(th);                        // Move the missile 1 tic
}

//----------------------------------------------------------------------------------------------------------------------
// Launch a player's missile (BFG, Rocket); tries to aim at a nearby monster.
//----------------------------------------------------------------------------------------------------------------------
void SpawnPlayerMissile(mobj_t& source, const mobjinfo_t& info) noexcept {
    // See which target is to be aimed at, start by aiming directly in the angle of the player
    angle_t an = source.angle;
    Fixed slope = AimLineAttack(source, an, 16 * 64 * FRACUNIT);

    if (!gpLineTarget) {    // No target?
        an += 1 << 26;      // Aim a little to the right
        slope = AimLineAttack(source, an, 16 * 64 * FRACUNIT);

        if (!gpLineTarget) {    // Still no target?
            an -= 2 << 26;      // Try a little to the left
            slope = AimLineAttack(source, an, 16 * 64 * FRACUNIT);

            if (!gpLineTarget) {        // I give up, just fire directly ahead
                an = source.angle;      // Reset the angle
                slope = 0;              // No z slope
            }
        }
    }

    const Fixed x = source.x;                       // From the player
    const Fixed y = source.y;
    const Fixed z = source.z + (32 * FRACUNIT);     // Off the ground

    mobj_t& th = SpawnMObj(x, y, z, info);          // Spawn the missile
    S_StartSound(&source.x, info.seesound);         // Play the sound
    th.target = &source;                            // Set myself as the target
    th.angle = an;                                  // Set the angle

    uint32_t speed = info.Speed;    // Get the missile speed
    an >>= ANGLETOFINESHIFT;        // Convert to index

    th.momx = (Fixed) speed * gFineCosine[an];
    th.momy = (Fixed) speed * gFineSine[an];
    th.momz = (Fixed) speed * slope;

    P_CheckMissileSpawn(th);    // Move the missile a little
}
