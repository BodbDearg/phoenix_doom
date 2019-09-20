#include "Map.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Random.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "MapData.h"
#include "MapUtil.h"
#include "Sight.h"
#include "Specials.h"
#include "Switch.h"
#include "Things/Info.h"
#include "Things/Interactions.h"
#include "Things/MapObj.h"
#include "Things/Move.h"
#include "Things/Shoot.h"
#include <algorithm>

mobj_t*     gpLineTarget;
mobj_t*     gpTmpThing;
Fixed       gTmpX;
Fixed       gTmpY;
bool        gbCheckPosOnly;
mobj_t*     gpShooter;
angle_t     gAttackAngle;
Fixed       gAttackRange;
Fixed       gAimTopSlope;
Fixed       gAimBottomSlope;

static int32_t      gUseBBox[4];        // Local box for BSP traversing
static vector_t     gUseLine;           // Temp subdivided line for targeting
static line_t*      gpCloseLine;        // Line to target
static Fixed        gCloseDist;         // Distance to target
static mobj_t*      gpBombSource;       // Explosion source
static mobj_t*      gpBombSpot;         // Explosion position
static uint32_t     gBombDamage;        // Damage done by explosion

//----------------------------------------------------------------------------------------------------------------------
// Input:
//  tmthing a mobj_t (can be valid or invalid)
//  tmx,tmy a position to be checked (doesn't need relate to the mobj_t->x,y)
//
// Output:
//  newsubsec   Subsector of the new position
//  floatok     if true, move would be ok if within tmfloorz - tmceilingz
//  floorz      New floor z
//  ceilingz    New ceiling z
//  tmdropoffz  the lowest point contacted (monsters won't move to a dropoff)
//  movething   thing collision
//----------------------------------------------------------------------------------------------------------------------
bool P_CheckPosition(mobj_t& thing, const Fixed x, const Fixed y) noexcept {
    gpTmpThing = &thing;        // Copy parms to globals
    gTmpX = x;
    gTmpY = y;
    gbCheckPosOnly = true;      // Only check the position
    P_TryMove2();               // See if I can move there...
    return gbTryMove2;          // Return the result
}

//----------------------------------------------------------------------------------------------------------------------
// Try to move to a new position and trigger special events.
//----------------------------------------------------------------------------------------------------------------------
bool P_TryMove(mobj_t& thing, const Fixed x, const Fixed y) noexcept {
    gpTmpThing = &thing;    // Source xy
    gTmpX = x;              // New x,y
    gTmpY = y;
    P_TryMove2();           // Move to the new spot

    // Pick up the specials
    mobj_t* pLatchedMoveThing = gpMoveThing;

    if (pLatchedMoveThing) { // Hit something?
        if ((thing.flags & MF_MISSILE) != 0) {
            // Missile bash into a monster
            const uint32_t damage = (Random::nextU32(7) + 1) * thing.InfoPtr->damage;      // Ouch!
            DamageMObj(*pLatchedMoveThing, &thing, thing.target, damage);
        }
        else if ((thing.flags & MF_SKULLFLY) != 0) {
            // Skull bash into a monster
            const uint32_t damage = (Random::nextU32(7) + 1) * thing.InfoPtr->damage;
            DamageMObj(*pLatchedMoveThing, &thing, &thing, damage);
            thing.flags &= ~MF_SKULLFLY;                            // Stop the skull from flying
            thing.momx = thing.momy = thing.momz = 0;               // No momentum
            SetMObjState(thing, thing.InfoPtr->spawnstate);         // Reset state
        } else {
            // Try to pick it up
            TouchSpecialThing(*pLatchedMoveThing, thing);
        }
    }

    return gbTryMove2;
}

//----------------------------------------------------------------------------------------------------------------------
// Routine used by BlockLinesIterator to check for line collision.
// I always return TRUE to check ALL lines.
//----------------------------------------------------------------------------------------------------------------------
static bool PIT_UseLines(line_t& li) noexcept {
    // Check bounding box first
    if ((gUseBBox[BOXRIGHT] <= li.bbox[BOXLEFT]) ||  // Within the bounding box?
        (gUseBBox[BOXLEFT] >= li.bbox[BOXRIGHT]) ||
        (gUseBBox[BOXTOP] <= li.bbox[BOXBOTTOM]) ||
        (gUseBBox[BOXBOTTOM] >= li.bbox[BOXTOP])
    ) {
        return true;    // Nope, they don't collide
    }

    // Find distance along usetrace
    vector_t dl;
    MakeVector(li, dl);                                     // Convert true line to a divline struct
    const Fixed frac = InterceptVector(gUseLine, dl);       // How much do they intercept
    if ((frac < 0) || (frac > gCloseDist)) {                // Behind source or too far away?
        return true;                                        // No collision
    }

    // The line is actually hit, find the distance
    if (li.special <= 0) {          // Not a special line?
        if (LineOpening(li)) {      // See if it passes through
            return true;            // keep going
        }
    }

    gpCloseLine = &li;      // This is the line of travel
    gCloseDist = frac;      // This is the length of the line
    return true;            // Can't use for than one special line in a row
}

//----------------------------------------------------------------------------------------------------------------------
// Looks for special lines in front of the player to activate.
// Used when the player presses "Use" to open a door or such.
//----------------------------------------------------------------------------------------------------------------------
void P_UseLines(player_t& player) noexcept {
    const uint32_t angle = player.mo->angle >> ANGLETOFINESHIFT;
    const Fixed x1 = player.mo->x;                                          // Get the source x,y
    const Fixed y1 = player.mo->y;
    const Fixed x2 = x1 + (USERANGE >> FRACBITS) * gFineCosine[angle];      // Get the dest X,Y
    const Fixed y2 = y1 + (USERANGE >> FRACBITS) * gFineSine[angle];

    gUseLine.x = x1;            // Create the useline record
    gUseLine.y = y1;
    gUseLine.dx = x2 - x1;      // Delta x and y
    gUseLine.dy = y2 - y1;

    if (gUseLine.dx >= 0) {
        gUseBBox[BOXRIGHT] = x2;    // Create the bounding box
        gUseBBox[BOXLEFT] = x1;
    } else {
        gUseBBox[BOXRIGHT] = x1;
        gUseBBox[BOXLEFT] = x2;
    }

    if (gUseLine.dy >= 0) {         // Create the bounding box
        gUseBBox[BOXTOP] = y2;
        gUseBBox[BOXBOTTOM] = y1;
    } else {
        gUseBBox[BOXTOP] = y1;
        gUseBBox[BOXBOTTOM] = y2;
    }

    int32_t yh = (gUseBBox[BOXTOP] - gBlockMapOriginY) >> MAPBLOCKSHIFT;        // Bounding box
    int32_t yl = (gUseBBox[BOXBOTTOM] - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    int32_t xh = (gUseBBox[BOXRIGHT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int32_t xl = (gUseBBox[BOXLEFT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    ++xh;   // For < compare later
    ++yh;
    
    xl = std::max(xl, 0);
    yl = std::max(yl, 0);
    
    gpCloseLine = nullptr;      // No line found
    gCloseDist = FRACUNIT;      // 1.0 units distance
    ++gValidCount;              // Make unique sector mark

    // Check the lines
    if (xh >= 0 && yh >= 0) {
        for (uint32_t y = (uint32_t) yl; y < (uint32_t) yh; ++y) {
            for (uint32_t x = (uint32_t) xl; x < (uint32_t) xh; ++x) {
                BlockLinesIterator(x, y, PIT_UseLines);
            }
        }
    }

    // Check closest line
    if (gpCloseLine) {                                      // Line nearby?
        if (gpCloseLine->special <= 0) {                    // Is it special?
            S_StartSound(&player.mo->x, sfx_noway);         // Make the grunt sound
        } else {
            P_UseSpecialLine(*player.mo, *gpCloseLine);     // Activate the special
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Routine used by BlockThingsIterator to check for damage to anyone within range of an explosion.
// Source is the creature that caused the explosion at spot.
// Always returns TRUE to check ALL lines
//----------------------------------------------------------------------------------------------------------------------
static bool PIT_RadiusAttack(mobj_t& thing) noexcept {
    ASSERT(gpBombSpot);

    if ((thing.flags & MF_SHOOTABLE) == 0) {    // Can this item be hit?
        return true;                            // Next thing...
    }

    const Fixed dx = std::abs(thing.x - gpBombSpot->x);     // Absolute distance from BOOM
    const Fixed dy = std::abs(thing.y - gpBombSpot->y);
    Fixed dist = (dx >= dy) ? dx : dy;                      // Get the greater of the two
    dist = (dist - thing.radius) >> FRACBITS;

    if (dist < 0) {     // Within the blast?
        dist = 0;       // Fix the distance
    }

    // Is the blast within range?
    if (dist < (int32_t) gBombDamage) {
        // DC: Bugfix to the original 3DO Doom: check for line of sight before applying damage.
        // In the original 3DO Doom you could damage stuff through walls with rockets! (wasn't like that in PC Doom)
        if (CheckSight(thing, *gpBombSpot)) {
            DamageMObj(thing, gpBombSpot, gpBombSource, gBombDamage - (uint32_t) dist);
        }
    }

    return true;    // Continue
}

//----------------------------------------------------------------------------------------------------------------------
// Inflict damage to all items within blast range.
// Source is the creature that casued the explosion at spot.
//----------------------------------------------------------------------------------------------------------------------
void RadiusAttack(mobj_t& spot, mobj_t* source, const uint32_t damage) noexcept {
    const Fixed dist = intToFixed16((int32_t) damage);

    int32_t yh = (spot.y + dist - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    int32_t yl = (spot.y - dist - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    int32_t xh = (spot.x + dist - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int32_t xl = (spot.x - dist - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    ++xh;
    ++yh;
    
    gpBombSpot = &spot;         // Copy to globals so PIT_Radius can see it
    gpBombSource = source;
    gBombDamage = damage;

    // Damage all things in collision range
    xl = std::max(xl, 0);
    yl = std::max(yl, 0);
    
    if (yh >= 0 && xh >= 0) {
        for (uint32_t y = (uint32_t) yl; y < (uint32_t) yh; ++y) {
            for (uint32_t x = (uint32_t) xl; x < (uint32_t) xh; ++x) {
                BlockThingsIterator(x, y, PIT_RadiusAttack);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Choose a target based of direction of facing.
// A line will be traced from the middle of shooter in the direction of attackangle until either a shootable
// mobj is within the visible aimtopslope / aimbottomslope range, or a solid wall blocks further tracing.
// If no thing is targeted along the entire range, the first line that blocks the midpoint of the trace will be hit.
//----------------------------------------------------------------------------------------------------------------------
Fixed AimLineAttack(mobj_t& t1, const angle_t angle, const Fixed distance) noexcept {
    gpShooter = &t1;
    gAttackRange = distance;
    gAttackAngle = angle;
    gAimTopSlope = 100 * FRACUNIT / 160;    // Can't shoot outside view angles
    gAimBottomSlope = -100 * FRACUNIT / 160;
    ++gValidCount;

    P_Shoot2();     // Call other code
    gpLineTarget = gpShootMObj;

    if (gpLineTarget) {         // Was there a valid hit?
        return gShootSlope;     // Return the slope of target
    }

    return 0;   // No target
}

//----------------------------------------------------------------------------------------------------------------------
// Actually perform an attack based off of the direction facing.
// Use by pistol, chain gun and shotguns.
// If slope == FIXED_MAX, use screen bounds for attacking.
//----------------------------------------------------------------------------------------------------------------------
void LineAttack(
    mobj_t& t1,
    const angle_t angle,
    const Fixed distance,
    const Fixed slope,
    const uint32_t damage
) noexcept {
    gpShooter = &t1;
    gAttackRange = distance;
    gAttackAngle = angle;

    if (slope == FRACMAX) {
        gAimTopSlope = 100 * FRACUNIT / 160;        // Can't shoot outside view angles
        gAimBottomSlope = -100 * FRACUNIT / 160;
    } else {
        gAimTopSlope = slope + 1;
        gAimBottomSlope = slope - 1;
    }

    ++gValidCount;
    P_Shoot2();                     // Perform the calculations
    gpLineTarget = gpShootMObj;     // Get the result

    const Fixed shootx2 = gShootX;
    const Fixed shooty2 = gShootY;
    const Fixed shootz2 = gShootZ;
    line_t* const pShootline2 = gpShootLine;

    // Shooting things: did we hit a thing?
    if (gpLineTarget) {
        if (gpLineTarget->flags & MF_NOBLOOD) {
            P_SpawnPuff(shootx2, shooty2, shootz2);             // Make a spark on the target
        } else {
            P_SpawnBlood(shootx2, shooty2, shootz2, damage);    // Squirt some blood!
        }

        DamageMObj(*gpLineTarget, &t1, &t1, damage);    // Do the damage
        return;
    }

    // Shooting walls: did we hit a wall?
    if (pShootline2) {
        if (pShootline2->special) {                 // Special
            P_ShootSpecialLine(t1, *pShootline2);   // Open a door or switch?
        }

        if (pShootline2->frontsector->CeilingPic == UINT32_MAX) {
            if (shootz2 > pShootline2->frontsector->ceilingheight) {
                return;     // Don't shoot the sky!
            }

            if (pShootline2->backsector && pShootline2->backsector->CeilingPic == UINT32_MAX) {
                return;     // it's a sky hack wall
            }
        }

        P_SpawnPuff(shootx2, shooty2, shootz2);     // Make a puff of smoke
    }
}
