#include "Base.h"

#include "Enemy.h"
#include "Game/Data.h"
#include "Game/Tick.h"
#include "Info.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Map/Sight.h"
#include "MapObj.h"

static mobj_t*          gpCheckThingMo;         // Used for PB_CheckThing
static Fixed            gTestX;
static Fixed            gTestY;
static Fixed            gTestFloorZ;
static Fixed            gTestCeilingZ;
static Fixed            gTestDropoffZ;
static subsector_t*     gpTestSubSec;
static line_t*          gpCeilingLine;
static mobj_t*          gpHitThing;
static Fixed            gTestBBox[4];           // Bounding box for tests
static int              gTestFlags;

//----------------------------------------------------------------------------------------------------------------------
// Float up or down at a set speed, used by flying monsters
//----------------------------------------------------------------------------------------------------------------------
static void FloatChange(mobj_t& mo) noexcept {
    ASSERT(mo.target);
    mobj_t& target = *mo.target;                                                // Get the target object
    Fixed delta = (target.z + (mo.height >> 1)) - mo.z;                         // Get the height differance
    const Fixed dist = GetApproxDistance(target.x - mo.x, target.y - mo.y);     // Distance to target
    delta = delta * 3;                                                          // Mul by 3 for a fudge factor

    if (delta < 0) {                // Delta is signed...
        if (dist < -delta) {        // Negate
            mo.z -= FLOATSPEED;     // Adjust the height
        }
        return;
    }

    if (dist < delta) {         // Normal compare
        mo.z += FLOATSPEED;     // Adjust the height
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Move a critter in the Z axis
//----------------------------------------------------------------------------------------------------------------------
static bool P_ZMovement(mobj_t& mo) noexcept {
    // DC: Note: added a division by 2 here to account for the move from a 35Hz timebase (PC) to a 60Hz timebase (3DO).
    // This fixes issues in the 3DO version with certain projectiles like imps fireballs moving too quickly.
    //
    // Apply basic z motion:
    mo.z += fixed16Div(mo.momz, 2 * FRACUNIT);    

    if (((mo.flags & MF_FLOAT) != 0) && mo.target) {    // float down towards target if too close
        FloatChange(mo);
    }

    // Clip movement
    if (mo.z <= mo.floorz) {    // Hit the floor
        if (mo.momz < 0) {
            mo.momz = 0;
        }
        mo.z = mo.floorz;

        if ((mo.flags & MF_MISSILE) != 0) {
            ExplodeMissile(mo);
            return true;
        }
    } else if ((mo.flags & MF_NOGRAVITY) == 0) {
        if (mo.momz == 0) {             // Not falling
            mo.momz = -GRAVITY * 2;     // Fall faster
        } else {
            mo.momz -= GRAVITY;
        }
    }

    if (mo.z + mo.height > mo.ceilingz) {   // Hit the ceiling
        if (mo.momz > 0) {
            mo.momz = 0;
        }
        mo.z = mo.ceilingz - mo.height;
        if ((mo.flags & MF_MISSILE) != 0) {
            ExplodeMissile(mo);
            return true;
        }
    }

    return false;
}

static bool PB_BoxCrossLine(line_t& ld) noexcept {    
    if ((gTestBBox[BOXRIGHT] <= ld.bbox[BOXLEFT]) ||
        (gTestBBox[BOXLEFT] >= ld.bbox[BOXRIGHT]) ||
        (gTestBBox[BOXTOP] <= ld.bbox[BOXBOTTOM]) ||
        (gTestBBox[BOXBOTTOM] >= ld.bbox[BOXTOP])
    ) {
        return false;
    }

    Fixed x1;
    Fixed x2;
    
    if (ld.slopetype == ST_POSITIVE) {
        x1 = gTestBBox[BOXLEFT];
        x2 = gTestBBox[BOXRIGHT];
    } else {
        x1 = gTestBBox[BOXRIGHT];
        x2 = gTestBBox[BOXLEFT];
    }

    const Fixed lx = ld.v1.x;
    const Fixed ly = ld.v1.y;
    const Fixed ldx = (ld.v2.x - ld.v1.x) >> 16;
    const Fixed ldy = (ld.v2.y - ld.v1.y) >> 16;

    const Fixed dx1 = (x1 - lx) >> 16;
    const Fixed dy1 = (gTestBBox[BOXTOP] - ly) >> 16;
    const Fixed dx2 = (x2 - lx) >> 16;
    const Fixed dy2 = (gTestBBox[BOXBOTTOM] - ly) >> 16;

    const bool bSide1 = (ldy * dx1 < dy1 * ldx);
    const bool bSide2 = (ldy * dx2 < dy2 * ldx);
    return (bSide1 != bSide2);
}

//----------------------------------------------------------------------------------------------------------------------
// Adjusts testfloorz and testceilingz as lines are contacted
//----------------------------------------------------------------------------------------------------------------------
static bool PB_CheckLine(line_t& ld) noexcept {
    Fixed opentop, openbottom;
    Fixed lowfloor;
    sector_t    *front, *back;

    // The moving thing's destination position will cross the given line.
    // If this should not be allowed, return FALSE.
    if (!ld.backsector) {
        return false;   // One sided line
    }

    if (((gTestFlags & MF_MISSILE) == 0) && ((ld.flags & (ML_BLOCKING|ML_BLOCKMONSTERS)) != 0)) {
        return false;   // Explicitly blocking
    }
    
    front = ld.frontsector;
    back = ld.backsector;

    if (front->ceilingheight < back->ceilingheight) {
        opentop = front->ceilingheight;
    } else {
        opentop = back->ceilingheight;
    }
    
    if (front->floorheight > back->floorheight) {
        openbottom = front->floorheight;
        lowfloor = back->floorheight;
    } else {
        openbottom = back->floorheight;
        lowfloor = front->floorheight;
    }

    // Adjust floor / ceiling heights
    if (opentop < gTestCeilingZ) {
        gTestCeilingZ = opentop;
        gpCeilingLine = &ld;
    }

    if (openbottom > gTestFloorZ) {
        gTestFloorZ = openbottom;
    }

    if (lowfloor < gTestDropoffZ) {
        gTestDropoffZ = lowfloor;
    }

    return true;
}

static bool PB_CrossCheck(line_t& ld) noexcept {
    if (PB_BoxCrossLine(ld)) {
        if (!PB_CheckLine(ld)) {
            return false;
        }
    }
    return true;
}

static bool PB_CheckThing(mobj_t& thing) noexcept {
    if ((thing.flags & MF_SOLID) == 0) {
        return true;
    }

    mobj_t& mo = *gpCheckThingMo;
    const Fixed blockdist = thing.radius + mo.radius;
    Fixed delta = thing.x - gTestX;

    if (delta < 0) {
        delta = -delta;
    }

    if (delta >= blockdist) {
        return true;    // Didn't hit it
    }

    delta = thing.y - gTestY;

    if (delta < 0) {
        delta = -delta;
    }

    if (delta >= blockdist) {
        return true;    // Didn't hit it
    }

    if (&thing == &mo) {
        return true;    // Don't clip against self
    }
    
    // Check for skulls slamming into things
    if ((gTestFlags & MF_SKULLFLY) != 0) {
        gpHitThing = &thing;
        return false;   // Stop moving
    }

    // Missiles can hit other things
    if ((gTestFlags & MF_MISSILE) != 0) {
        // See if it went over / under
        if (mo.z > thing.z + thing.height) {
            return true;    // Overhead
        }

        if (mo.z + mo.height < thing.z) {
            return true;    // Underneath
        }

        if (mo.target->InfoPtr == thing.InfoPtr) {    // don't hit same species as originator
            if (&thing == mo.target) {
                return true;    // Don't explode on shooter
            }

            if (thing.InfoPtr != &gMObjInfo[MT_PLAYER]) {
                return false;   // Explode, but do no damage
            }
            
            // Let players missile other players...
        }

        if ((thing.flags & MF_SHOOTABLE) == 0) {
            return ((thing.flags & MF_SOLID) == 0);     // Didn't do any damage
        }

        // Damage / explode
        gpHitThing = &thing;
        return false;           // Don't traverse any more
    }

    return ((thing.flags & MF_SOLID) == 0);
}

//----------------------------------------------------------------------------------------------------------------------
// This is purely informative, nothing is modified (except things picked up).
//
//  in:
//      basething       a mobj_t
//      testx,testy     a position to be checked (doesn't need relate to the mobj_t->x,y)
//  out:
//      testsubsec
//      floorz
//      ceilingz
//      testdropoffz    the lowest point contacted (monsters won't move to a dropoff)
//      hitthing
//----------------------------------------------------------------------------------------------------------------------
static bool PB_CheckPosition(mobj_t& mo) noexcept {
    gTestFlags = mo.flags;

    const Fixed radius = mo.radius;
    gTestBBox[BOXTOP] = gTestY + radius;
    gTestBBox[BOXBOTTOM] = gTestY - radius;
    gTestBBox[BOXRIGHT] = gTestX + radius;
    gTestBBox[BOXLEFT] = gTestX - radius;

    // The base floor / ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    gpTestSubSec = &PointInSubsector(gTestX, gTestY);
    gTestFloorZ = gTestDropoffZ = gpTestSubSec->sector->floorheight;
    gTestCeilingZ = gpTestSubSec->sector->ceilingheight;

    ++gValidCount;
    gpCeilingLine = nullptr;
    gpHitThing = nullptr;

    // The bounding box is extended by MAXRADIUS because mobj_ts are grouped into mapblocks based on their
    // origin point, and can overlap into adjacent blocks by up to MAXRADIUS units.
    int32_t xl = (gTestBBox[BOXLEFT] - gBlockMapOriginX - MAXRADIUS) >> MAPBLOCKSHIFT;
    int32_t xh = (gTestBBox[BOXRIGHT] - gBlockMapOriginX + MAXRADIUS) >> MAPBLOCKSHIFT;
    int32_t yl = (gTestBBox[BOXBOTTOM] - gBlockMapOriginY - MAXRADIUS) >> MAPBLOCKSHIFT;
    int32_t yh = (gTestBBox[BOXTOP] - gBlockMapOriginY + MAXRADIUS) >> MAPBLOCKSHIFT;

    if (xl < 0) {
        xl = 0;
    }

    if (yl < 0) {
        yl = 0;
    }

    if (xh >= (int32_t) gBlockMapWidth) {
        xh = (int32_t) gBlockMapWidth - 1;
    }

    if (yh >= (int32_t) gBlockMapHeight) {
        yh = (int32_t) gBlockMapHeight - 1;
    }

    gpCheckThingMo = &mo;   // Store for PB_CheckThing

    for (int32_t bx = xl; bx <= xh; bx++) {
        for (int32_t by = yl; by <= yh; by++) {
            if (!BlockThingsIterator(bx, by, PB_CheckThing))
                return false;
            
            if (!BlockLinesIterator(bx, by, PB_CrossCheck))
                return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Attempt to move to a new position
//----------------------------------------------------------------------------------------------------------------------
static bool PB_TryMove(const Fixed tryx, const Fixed tryy, mobj_t& mo) noexcept {
    gTestX = tryx;
    gTestY = tryy;

    if (!PB_CheckPosition(mo))
        return false;   // Solid wall or thing

    if (gTestCeilingZ - gTestFloorZ < mo.height)
        return false;   // Doesn't fit

    if (gTestCeilingZ - mo.z < mo.height)
        return false;   // Mobj must lower itself to fit
    
    if (gTestFloorZ - mo.z > 24 * FRACUNIT)
        return false;   // Too big a step up
    
    if (((gTestFlags & (MF_DROPOFF|MF_FLOAT)) == 0) && (gTestFloorZ - gTestDropoffZ > 24 * FRACUNIT))
        return false;   // Don't stand over a dropoff
    
    // The move is ok, so link the thing into its new position
    UnsetThingPosition(mo);
    mo.floorz = gTestFloorZ;
    mo.ceilingz = gTestCeilingZ;
    mo.x = tryx;
    mo.y = tryy;
    SetThingPosition(mo);
    return true;
}

static constexpr Fixed STOPSPEED    = 0x1000;
static constexpr Fixed FRICTION     = 0xd240;

static Fixed capMoveAmountToMax(const Fixed moveAmount) noexcept {
    if (moveAmount < 0) {
        return (moveAmount >= -MAXMOVE) ? moveAmount : -MAXMOVE;
    } else {
        return (moveAmount <= MAXMOVE) ? moveAmount : MAXMOVE;
    }
}

static bool P_XYMovement(mobj_t& mo) noexcept {   
    // DC: Note: added a division by 2 here to account for the move from a 35Hz timebase (PC) to a 60Hz timebase (3DO).
    // This fixes issues in the 3DO version with certain projectiles like imps fireballs moving too quickly.
    Fixed xleft = mo.momx & ~7;
    Fixed yleft = mo.momy & ~7;
    xleft = fixed16Div(mo.momx, 2 * FRACUNIT);
    yleft = fixed16Div(mo.momy, 2 * FRACUNIT);

    while ((xleft != 0) || (yleft != 0)) {
        // Cut the move into chunks if too large
        Fixed xuse = capMoveAmountToMax(xleft);
        Fixed yuse = capMoveAmountToMax(yleft);
        xleft -= xuse;
        yleft -= yuse;

        if (!PB_TryMove(mo.x + xuse, mo.y + yuse, mo)) {
            // Blocked move
            if ((mo.flags & MF_SKULLFLY) != 0) {
                L_SkullBash(mo, gpHitThing);
                return true;
            }

            if (mo.flags & MF_MISSILE) {    // Explode a missile
                if (gpCeilingLine && gpCeilingLine->backsector && (gpCeilingLine->backsector->CeilingPic == -1)) {
                    // Hack to prevent missiles exploding against the sky
                    P_RemoveMobj(mo);
                    return true;
                }

                L_MissileHit(mo, gpHitThing);
                return true;
            }

            mo.momx = mo.momy = 0;
            return false;
        }
    }

    // Slow down
    if ((mo.flags & (MF_MISSILE|MF_SKULLFLY)) != 0) {
        return false;   // No friction for missiles ever
    }

    if (mo.z > mo.floorz) {
        return false;   // No friction when airborne
    }

    if (mo.flags & MF_CORPSE) {
        if (mo.floorz != mo.subsector->sector->floorheight) {
            return false;   // Don't stop halfway off a step
        }
    }

    if ((mo.momx > -STOPSPEED) &&
        (mo.momx < STOPSPEED) &&
        (mo.momy > -STOPSPEED) &&
        (mo.momy < STOPSPEED)
    ) {
        mo.momx = 0;
        mo.momy = 0;
    } else {
        mo.momx = (mo.momx >> 8) * (FRICTION >> 8);
        mo.momy = (mo.momy >> 8) * (FRICTION >> 8);
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// Process all the critter logic
//----------------------------------------------------------------------------------------------------------------------
static void P_MobjThinker(mobj_t& mobj) noexcept {
    if ((mobj.momx != 0) || (mobj.momy != 0)) {     // Any horizontal momentum?
        if (P_XYMovement(mobj)) {                   // Move it
            return;                                 // Exit if state change
        }
    }

    if ((mobj.z != mobj.floorz) || mobj.momz) {     // Any z momentum?
        if (P_ZMovement(mobj)) {                    // Move it (Or fall)
            return;                                 // Exit if state change
        }
    }

    bool bSightFlag = true;

    if (mobj.tics == -1) {  // Never time out?
        return;
    }

    if (mobj.tics > 1) {
        --mobj.tics;        // Count down
        return;
    }

    if (bSightFlag && ((mobj.flags & MF_COUNTKILL) != 0)) {     // Can it be killed?
        bSightFlag = false;
        mobj.flags &= ~MF_SEETARGET;                            // Assume I don't see a target
        if (mobj.target) {                                      // Do I have a target?
            if (CheckSight(mobj, *mobj.target)) {
                mobj.flags |= MF_SEETARGET;
            }
        }
    }

    SetMObjState(mobj, mobj.state->nextstate);  // Next object state
}

//----------------------------------------------------------------------------------------------------------------------
// Execute base think logic for the critters every tic
//----------------------------------------------------------------------------------------------------------------------
void P_RunMobjBase() noexcept {
    mobj_t* pMObj = gMObjHead.next;
    while (pMObj != &gMObjHead) {
        mobj_t* const pNext = pMObj->next;      // In case it's deleted!
        if (!pMObj->player) {                   // Don't handle players
            P_MobjThinker(*pMObj);              // Execute the code
        }
        pMObj = pNext;                          // Next in the list
    }
}
