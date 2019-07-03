#include "Map.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Random.h"
#include "Base/Tables.h"
#include "Game/Data.h"
#include "MapData.h"
#include "MapUtil.h"
#include "Specials.h"
#include "Switch.h"
#include "Things/Info.h"
#include "Things/Interactions.h"
#include "Things/MapObj.h"
#include "Things/Move.h"
#include "Things/Shoot.h"
#include <cstdlib>

mobj_t*     gLineTarget;
mobj_t*     gTmpThing;
Fixed       gTmpX;
Fixed       gTmpY;
bool        gCheckPosOnly;
mobj_t*     gShooter;
angle_t     gAttackAngle;
Fixed       gAttackRange;
Fixed       gAimTopSlope;
Fixed       gAimBottomSlope;

static int32_t      gUseBBox[4];    // Local box for BSP traversing
static vector_t     gUseLine;       // Temp subdivided line for targeting
static line_t*      gCloseLine;     // Line to target
static Fixed        gCloseDist;     // Distance to target
static mobj_t*      gBombSource;    // Explosion source
static mobj_t*      gBombSpot;      // Explosion position
static uint32_t     gBombDamage;    // Damage done by explosion

/**********************************

Input:
tmthing a mobj_t (can be valid or invalid)
tmx,tmy a position to be checked (doesn't need relate to the mobj_t->x,y)

Output:
newsubsec   Subsector of the new position
floatok     if true, move would be ok if within tmfloorz - tmceilingz
floorz      New floor z
ceilingz    New ceiling z
tmdropoffz  the lowest point contacted (monsters won't move to a dropoff)
movething   thing collision

**********************************/

bool P_CheckPosition(mobj_t *thing, Fixed x, Fixed y)
{
    gTmpThing = thing;          // Copy parms to globals
    gTmpX = x;
    gTmpY = y;
    gCheckPosOnly = true;       // Only check the position
    P_TryMove2();               // See if I can move there...
    return gTryMove2;           // Return the result
}

/**********************************

    Try to move to a new position and trigger special events

**********************************/

bool P_TryMove(mobj_t *thing, Fixed x, Fixed y)
{
    uint32_t damage;
    mobj_t *latchedmovething;

    gTmpThing = thing;        // Source xy
    gTmpX = x;                // New x,y
    gTmpY = y;
    P_TryMove2();           // Move to the new spot

// Pick up the specials

    latchedmovething = gMoveThing;      // Hit something?

    if (latchedmovething) {
        // missile bash into a monster
        if (thing->flags & MF_MISSILE) {
            damage = (Random::nextU32(7)+1)*thing->InfoPtr->damage;   // Ouch!
            DamageMObj(latchedmovething,thing,thing->target,damage);
        // skull bash into a monster
        } else if (thing->flags & MF_SKULLFLY) {
            damage = (Random::nextU32(7)+1)*thing->InfoPtr->damage;
            DamageMObj(latchedmovething,thing,thing,damage);
            thing->flags &= ~MF_SKULLFLY;       // Stop the skull from flying
            thing->momx = thing->momy = thing->momz = 0;    // No momentum
            SetMObjState(thing,thing->InfoPtr->spawnstate); // Reset state
        // Try to pick it up
        } else {
            TouchSpecialThing(latchedmovething,thing);
        }
    }
    return gTryMove2;       // Return result
}

/**********************************

    Routine used by BlockLinesIterator to check for
    line collision. I always return TRUE to check ALL lines

**********************************/

static uint32_t PIT_UseLines(line_t *li)
{
    vector_t dl;
    Fixed frac;

// check bounding box first

    if (gUseBBox[BOXRIGHT] <= li->bbox[BOXLEFT]  // Within the bounding box?
    ||  gUseBBox[BOXLEFT] >= li->bbox[BOXRIGHT]
    ||  gUseBBox[BOXTOP] <= li->bbox[BOXBOTTOM]
    ||  gUseBBox[BOXBOTTOM] >= li->bbox[BOXTOP] ) {
        return true;            // Nope, they don't collide
    }

// find distance along usetrace

    MakeVector(li,&dl);         // Convert true line to a divline struct
    frac = InterceptVector(&gUseLine,&dl);       // How much do they intercept
    if ((frac < 0) ||           // Behind source?
        (frac > gCloseDist)) {   // Too far away?
        return true;        // No collision
    }

// The line is actually hit, find the distance

    if (!li->special) {     // Not a special line?
        if (LineOpening(li)) {  // See if it passes through
            return true;    // keep going
        }
    }
    gCloseLine = li;     // This is the line of travel
    gCloseDist = frac;   // This is the length of the line
    return true;        // Can't use for than one special line in a row
}

//---------------------------------------------------------------------------------------------------------------------
// Looks for special lines in front of the player to activate.
// Used when the player presses "Use" to open a door or such.
//---------------------------------------------------------------------------------------------------------------------
void P_UseLines(player_t* player) {
    uint32_t angle = player->mo->angle >> ANGLETOFINESHIFT;
    Fixed x1 = player->mo->x;                                       // Get the source x,y
    Fixed y1 = player->mo->y;
    Fixed x2 = x1 + (USERANGE >> FRACBITS) * gFineCosine[angle];    // Get the dest X,Y
    Fixed y2 = y1 + (USERANGE >> FRACBITS) * gFineSine[angle];
    
    gUseLine.x = x1;        // Create the useline record
    gUseLine.y = y1;
    gUseLine.dx = x2-x1;    // Delta x and y
    gUseLine.dy = y2-y1;

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

    int yh = (gUseBBox[BOXTOP] - gBlockMapOriginY) >> MAPBLOCKSHIFT;        // Bounding box
    int yl = (gUseBBox[BOXBOTTOM] - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    int xh = (gUseBBox[BOXRIGHT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int xl = (gUseBBox[BOXLEFT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    
    ++xh;   // For < compare later
    ++yh;

    gCloseLine = 0;             // No line found
    gCloseDist = FRACUNIT;      // 1.0 units distance
    ++gValidCount;              // Make unique sector mark
    
    {
        int y = yl;
        do {
            int x = xl;
            do {
                BlockLinesIterator(x,y,PIT_UseLines);   // Check the lines
            } while (++x < xh);
        } while (++y < yh);
    }
    
    // Check closest line
    if (gCloseLine) {                                   // Line nearby?
        if (!gCloseLine->special) {                     // Is it special?
            S_StartSound(&player->mo->x, sfx_noway);    // Make the grunt sound
        } else {
            P_UseSpecialLine(player->mo, gCloseLine);   // Activate the special
        }
    }
}

/**********************************

    Routine used by BlockThingsIterator to check for
    damage to anyone within range of an explosion.
    Source is the creature that caused the explosion at spot
    Always return TRUE to check ALL lines

**********************************/

static uint32_t PIT_RadiusAttack(mobj_t *thing)
{
    Fixed dx,dy,dist;

    if (!(thing->flags & MF_SHOOTABLE) ) {      // Can this item be hit?
        return true;            // Next thing...
    }

    dx = abs(thing->x - gBombSpot->x);      // Absolute distance from BOOM
    dy = abs(thing->y - gBombSpot->y);
    dist = dx>=dy ? dx : dy;        // Get the greater of the two
    dist = (dist - thing->radius) >> FRACBITS;
    if (dist < 0) {     // Within the blast?
        dist = 0;       // Fix the distance
    }
    if (dist < gBombDamage) {        // Within blast range?
        DamageMObj(thing,gBombSpot,gBombSource,gBombDamage-dist);
    }
    return true;        // Continue
}

//---------------------------------------------------------------------------------------------------------------------
// Inflict damage to all items within blast range.
// Source is the creature that casued the explosion at spot.
//---------------------------------------------------------------------------------------------------------------------
void RadiusAttack(mobj_t* spot, mobj_t* source, uint32_t damage) {
    Fixed dist = damage << FRACBITS;    // Convert to fixed
    
    uint32_t yh = (spot->y + dist - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    uint32_t yl = (spot->y - dist - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    uint32_t xh = (spot->x + dist - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    uint32_t xl = (spot->x - dist - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    ++xh;
    ++yh;
    
    gBombSpot = spot;           // Copy to globals so PIT_Radius can see it
    gBombSource = source;
    gBombDamage = damage;
    
    // Damage all things in collision range
    {
        uint32_t y = yl;
        do {
            uint32_t x = xl;
            do {
                BlockThingsIterator(x, y, PIT_RadiusAttack);
            } while (++x < xh);
        } while (++y < yh);
    }
}

/**********************************

    Choose a target based of direction of facing.
    A line will be traced from the middle of shooter in the direction of
    attackangle until either a shootable mobj is within the visible
    aimtopslope / aimbottomslope range, or a solid wall blocks further
    tracing.  If no thing is targeted along the entire range, the first line
    that blocks the midpoint of the trace will be hit.

**********************************/

Fixed AimLineAttack(mobj_t *t1,angle_t angle,Fixed distance)
{
    gShooter = t1;
    gAttackRange = distance;
    gAttackAngle = angle;
    gAimTopSlope = 100*FRACUNIT/160; // Can't shoot outside view angles
    gAimBottomSlope = -100*FRACUNIT/160;

    ++gValidCount;

    P_Shoot2();         // Call other code
    gLineTarget = gShootMObj;
    if (gLineTarget) {       // Was there a valid hit?
        return gShootSlope;  // Return the slope of target
    }
    return 0;       // No target
}

/**********************************

    Actually perform an attack based off of the direction facing.
    Use by pistol, chain gun and shotguns.
    If slope == FIXED_MAX, use screen bounds for attacking

**********************************/

void LineAttack(mobj_t *t1,angle_t angle,Fixed distance,Fixed slope, uint32_t damage)
{
    line_t *shootline2;
    int shootx2, shooty2, shootz2;

    gShooter = t1;
    gAttackRange = distance;
    gAttackAngle = angle;

    if (slope == FIXED_MAX) {
        gAimTopSlope = 100*FRACUNIT/160; // can't shoot outside view angles
        gAimBottomSlope = -100*FRACUNIT/160;
    } else {
        gAimTopSlope = slope+1;
        gAimBottomSlope = slope-1;
    }
    ++gValidCount;
    P_Shoot2();         // Perform the calculations
    gLineTarget = gShootMObj;     // Get the result
    shootline2 = gShootLine;
    shootx2 = gShootX;
    shooty2 = gShootY;
    shootz2 = gShootZ;

// Shoot thing
    if (gLineTarget) {           // Did you hit?
        if (gLineTarget->flags & MF_NOBLOOD) {
            P_SpawnPuff(shootx2,shooty2,shootz2);   // Make a spark on the target
        } else {
            P_SpawnBlood(shootx2,shooty2,shootz2,damage);   // Squirt some blood!
        }
        DamageMObj(gLineTarget,t1,t1,damage);        // Do the damage
        return;
    }

// Shoot wall

    if (shootline2) {       // Hit a wall?
        if (shootline2->special) {      // Special
            P_ShootSpecialLine(t1, shootline2); // Open a door or switch?
        }
        if (shootline2->frontsector->CeilingPic==-1) {
            if (shootz2 > shootline2->frontsector->ceilingheight) {
                return;     // don't shoot the sky!
            }
            if  (shootline2->backsector &&
                shootline2->backsector->CeilingPic==-1) {
                return;     // it's a sky hack wall
            }
        }
        P_SpawnPuff(shootx2,shooty2,shootz2);       // Make a puff of smoke
    }
}
