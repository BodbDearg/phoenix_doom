#include "Change.h"

#include "Base/Random.h"
#include "Game/Data.h"
#include "Game/Tick.h"
#include "Map.h"
#include "MapData.h"
#include "MapUtil.h"
#include "Things/Info.h"
#include "Things/Interactions.h"
#include "Things/MapObj.h"
#include "Things/Move.h"

//----------------------------------------------------------------------------------------------------------------------
// SECTOR HEIGHT CHANGING
//
// After modifying a sectors floor or ceiling height, call this routine to adjust the
// positions of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage as they are being crushed
// If Crunch is false, you should set the sector height back the way it was and call
// ChangeSector again to undo the changes
//----------------------------------------------------------------------------------------------------------------------

static bool gbCrushChange;      // If true, then crush bodies to blood
static bool gbNoFit;            // Set to true if something is blocking

//----------------------------------------------------------------------------------------------------------------------
// Takes a valid thing and adjusts the thing->floorz, thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters whenever a sector changes height.
//
// If the thing doesn't fit, the z will be set to the lowest value and false will be returned.
//----------------------------------------------------------------------------------------------------------------------
static bool ThingHeightClip(mobj_t& thing) noexcept {
    const bool bOnfloor = (thing.z == thing.floorz);    // Already on the floor?

    // Get the floor and ceilingz from the monsters position
    P_CheckPosition(thing, thing.x, thing.y);

    // What about stranding a monster partially off an edge?

    thing.floorz = gTmpFloorZ;       // Save off the variables
    thing.ceilingz = gTmpCeilingZ;

    if (bOnfloor) {                 // Walking monsters rise and fall with the floor
        thing.z = thing.floorz;     // Pin to the floor
    } else {
        // Don't adjust a floating monster unless forced to
        if (thing.z + thing.height > thing.ceilingz) {
            thing.z = thing.ceilingz - thing.height;
        }
    }

    if (thing.ceilingz - thing.floorz < thing.height) {
        return false;   // Can't fit!!
    } else {
        return true;    // I can fit!
    }
}

//----------------------------------------------------------------------------------------------------------------------
// This is called from BlockThingsIterator
//----------------------------------------------------------------------------------------------------------------------
static bool PIT_ChangeSector(mobj_t& thing) noexcept {
    if (ThingHeightClip(thing)) {       // Too small?
        return true;                    // Keep checking
    }

    // Crunch bodies to giblets
    if (thing.MObjHealth <= 0) {                    // No health...
        SetMObjState(thing, gStates[S_GIBS]);       // Change to goo
        thing.flags &= ~MF_SOLID;                   // No longer solid
        thing.height = 0;                           // No height
        thing.radius = 0;                           // No radius
        return true;                                // keep checking
    }

    // Crunch dropped items
    if ((thing.flags & MF_DROPPED) != 0) {
        P_RemoveMobj(thing);                    // Get rid of it
        return true;                            // Keep checking
    }

    if ((thing.flags & MF_SHOOTABLE) == 0) {    // Don't squash critters
        return true;                            // assume it is bloody gibs or something
    }

    gbNoFit = true;     // Can't fit

    if (gbCrushChange && gbTick4) {     // Crush it?
        DamageMObj(thing, 0, 0, 10);    // Take some damage

        // Spray blood in a random direction and have it jump out
        mobj_t& mo = SpawnMObj(thing.x, thing.y, thing.z + thing.height / 2, gMObjInfo[MT_BLOOD]);
        mo.momx = (255 - (Fixed) Random::nextU32(511)) << (FRACBITS - 4);
        mo.momy = (255 - (Fixed) Random::nextU32(511)) << (FRACBITS - 4);
    }

    return true;    // Keep checking (crush other things)
}

//----------------------------------------------------------------------------------------------------------------------
// Scan all items that are on a specific block to see if it can be crushed.
//----------------------------------------------------------------------------------------------------------------------
bool ChangeSector(sector_t& sector, bool bCrunch) noexcept {
    gPlayer.lastsoundsector = nullptr;      // Force next sound to reflood
    gbNoFit = false;                        // Assume that it's ok
    gbCrushChange = bCrunch;                // Can I crush bodies

    // Recheck heights for all things near the moving sector
    int32_t x2 = sector.blockbox[BOXRIGHT];
    int32_t y2 = sector.blockbox[BOXTOP];
    int32_t x = sector.blockbox[BOXLEFT];

    do {
        int32_t y = sector.blockbox[BOXBOTTOM];
        do {
            BlockThingsIterator(x, y, PIT_ChangeSector);    // Test everything
        } while (++y < y2);
    } while (++x < x2);

    return gbNoFit;     // Return flag
}
