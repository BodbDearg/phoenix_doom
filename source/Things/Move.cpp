#include "Move.h"

#include "Game/Data.h"
#include "Info.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"

bool        gbTryMove2;         // Result from P_TryMove2
bool        gbFloatOk;          // If true, move would be ok if within tmfloorz - tmceilingz
Fixed       gTmpFloorZ;         // Current floor z for P_TryMove2
Fixed       gTmpCeilingZ;       // Current ceiling z for P_TryMove2
mobj_t*     gpMoveThing;        // Either a skull/missile target or a special pickup
line_t*     gpBlockLine;        // Might be a door that can be opened

static Fixed            gOldX;
static Fixed            gOldY;
static Fixed            gTmpBBox[4];
static uint32_t         gTmpFlags;
static Fixed            gTmpDropoffZ;       // Lowest point contacted
static subsector_t*     gpNewSubSec;        // Dest subsector

//----------------------------------------------------------------------------------------------------------------------
// Attempt to move to a new position, crossing special lines unless MF_TELEPORT is set.
//----------------------------------------------------------------------------------------------------------------------
void P_TryMove2() noexcept {
    gbTryMove2 = false;     // Until proven otherwise
    gbFloatOk = false;

    gOldX = gpTmpThing->x;
    gOldY = gpTmpThing->y;
    PM_CheckPosition();

    if (gbCheckPosOnly) {
        gbCheckPosOnly = false;
        return;
    }

    if (!gbTryMove2) {
        return;
    }

    if ((gpTmpThing->flags & MF_NOCLIP) == 0) {
        gbTryMove2 = false;

        if (gTmpCeilingZ - gTmpFloorZ < gpTmpThing->height) {
            return;     // Doesn't fit
        }

        gbFloatOk = true;

        if (((gpTmpThing->flags & MF_TELEPORT) == 0) && 
            (gTmpCeilingZ - gpTmpThing->z < gpTmpThing->height)
        ) {
            return;     // Mobj must lower itself to fit
        }

        if (((gpTmpThing->flags & MF_TELEPORT) == 0) &&
            (gTmpFloorZ - gpTmpThing->z > 24 * FRACUNIT)
        ) {
            return;     // Too big a step up
        }

        if (((gpTmpThing->flags & (MF_DROPOFF|MF_FLOAT)) == 0) &&
            (gTmpFloorZ - gTmpDropoffZ > 24 * FRACUNIT)
        ) {
            return;     // Don't stand over a dropoff
        }
    }

    // The move is ok, so link the thing into its new position
    UnsetThingPosition(*gpTmpThing);

    gpTmpThing->floorz = gTmpFloorZ;
    gpTmpThing->ceilingz = gTmpCeilingZ;
    gpTmpThing->x = gTmpX;
    gpTmpThing->y = gTmpY;

    SetThingPosition(*gpTmpThing);
    gbTryMove2 = true;
    return;
}

static bool PM_CrossCheck(line_t& ld) noexcept {
    if (PM_BoxCrossLine (ld)) {
        if (!PIT_CheckLine(ld)) {
            return false;
        }
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// This is purely informative, nothing is modified (except things picked up).
//
// in:
//  tmthing     A mobj_t (can be valid or invalid)
//  tmx,tmy     A position to be checked (doesn't need relate to the mobj_t->x,y)
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz  The lowest point contacted (monsters won't move to a dropoff)
//  movething
//----------------------------------------------------------------------------------------------------------------------
void PM_CheckPosition() noexcept {
    gTmpFlags = gpTmpThing->flags;

    gTmpBBox[BOXTOP] = gTmpY + gpTmpThing->radius;
    gTmpBBox[BOXBOTTOM] = gTmpY - gpTmpThing->radius;
    gTmpBBox[BOXRIGHT] = gTmpX + gpTmpThing->radius;
    gTmpBBox[BOXLEFT] = gTmpX - gpTmpThing->radius;

    gpNewSubSec = &PointInSubsector(gTmpX,gTmpY);

    // The base floor / ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    gTmpFloorZ = gTmpDropoffZ = gpNewSubSec->sector->floorheight;
    gTmpCeilingZ = gpNewSubSec->sector->ceilingheight;

    ++gValidCount;

    gpMoveThing = nullptr;
    gpBlockLine = nullptr;

    if (gTmpFlags & MF_NOCLIP) {
        gbTryMove2 = true;
        return;
    }

    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS because mobj_ts are grouped into mapblocks based
    // on their origin point, and can overlap into adjacent blocks by up to MAXRADIUS units.
    int32_t xl = (gTmpBBox[BOXLEFT] - gBlockMapOriginX - MAXRADIUS) >> MAPBLOCKSHIFT;
    int32_t xh = (gTmpBBox[BOXRIGHT] - gBlockMapOriginX + MAXRADIUS) >> MAPBLOCKSHIFT;
    int32_t yl = (gTmpBBox[BOXBOTTOM] - gBlockMapOriginY - MAXRADIUS) >> MAPBLOCKSHIFT;
    int32_t yh = (gTmpBBox[BOXTOP] - gBlockMapOriginY + MAXRADIUS) >> MAPBLOCKSHIFT;

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

    for (uint32_t bx = (uint32_t) xl; bx <= (uint32_t) xh; bx++) {
        for (uint32_t by = (uint32_t) yl; by <= (uint32_t) yh; by++) {
            if (!BlockThingsIterator(bx, by, PIT_CheckThing)) {
                gbTryMove2 = false;
                return;
            }
        }
    }

    // Check lines
    xl = (gTmpBBox[BOXLEFT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    xh = (gTmpBBox[BOXRIGHT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    yl = (gTmpBBox[BOXBOTTOM] - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    yh = (gTmpBBox[BOXTOP] - gBlockMapOriginY) >> MAPBLOCKSHIFT;

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

    for (uint32_t bx = (uint32_t) xl; bx <= (uint32_t) xh; bx++) {
        for (uint32_t by = (uint32_t) yl; by <= (uint32_t) yh; by++) {
            if (!BlockLinesIterator(bx, by, PM_CrossCheck)) {
                gbTryMove2 = false;
                return;
            }
        }
    }

    gbTryMove2 = true;
    return;
}

bool PM_BoxCrossLine(line_t& ld) noexcept {    
    if ((gTmpBBox[BOXRIGHT] <= ld.bbox[BOXLEFT]) ||
        (gTmpBBox[BOXLEFT] >= ld.bbox[BOXRIGHT]) ||
        (gTmpBBox[BOXTOP] <= ld.bbox[BOXBOTTOM]) ||
        (gTmpBBox[BOXBOTTOM] >= ld.bbox[BOXTOP])
    ) {
        return false;
    }

    const Fixed y1 = gTmpBBox[BOXTOP];
    const Fixed y2 = gTmpBBox[BOXBOTTOM];
    Fixed x1;
    Fixed x2;

    if (ld.slopetype == ST_POSITIVE) {
        x1 = gTmpBBox[BOXLEFT];
        x2 = gTmpBBox[BOXRIGHT];
    } else {
        x1 = gTmpBBox[BOXRIGHT];
        x2 = gTmpBBox[BOXLEFT];
    }

    const Fixed lx = ld.v1.x;
    const Fixed ly = ld.v1.y;
    const Fixed ldx = (ld.v2.x - lx) >> 16;
    const Fixed ldy = (ld.v2.y - ly) >> 16;

    const Fixed dx1 = (x1 - lx) >> 16;
    const Fixed dy1 = (y1 - ly) >> 16;
    const Fixed dx2 = (x2 - lx) >> 16;
    const Fixed dy2 = (y2 - ly) >> 16;

    const bool bSide1 = (ldy * dx1) < (dy1 * ldx);
    const bool bSide2 = (ldy * dx2) < (dy2 * ldx);
    return (bSide1 != bSide2);
}

//----------------------------------------------------------------------------------------------------------------------
// Adjusts tmfloorz and tmceilingz as lines are contacted
//----------------------------------------------------------------------------------------------------------------------
bool PIT_CheckLine(line_t& ld) noexcept {
    // A line has been hit:
    // The moving thing's destination position will cross the given line.
    // If this should not be allowed, return false.
    if (!ld.backsector) {
        return false;   // One sided line
    }

    if ((gpTmpThing->flags & MF_MISSILE) == 0) {
        if ((ld.flags & ML_BLOCKING) != 0) {
            return false;   // Explicitly blocking everything
        }

        if ((!gpTmpThing->player) && ((ld.flags & ML_BLOCKMONSTERS) != 0)) {
            return false;   // Block monsters only
        }
    }

    sector_t& front = *ld.frontsector;
    sector_t& back = *ld.backsector;

    if ((front.ceilingheight == front.floorheight) ||
        (back.ceilingheight == back.floorheight)
    ) {
        gpBlockLine = &ld;
        return false;   // Probably a closed door
    }

    Fixed pm_opentop;
    
    if (front.ceilingheight < back.ceilingheight) {
        pm_opentop = front.ceilingheight;
    } else {
        pm_opentop = back.ceilingheight;
    }

    Fixed pm_openbottom;
    Fixed pm_lowfloor;

    if (front.floorheight > back.floorheight) {
        pm_openbottom = front.floorheight;
        pm_lowfloor = back.floorheight;
    } else {
        pm_openbottom = back.floorheight;
        pm_lowfloor = front.floorheight;
    }

    // Adjust floor / ceiling heights
    if (pm_opentop < gTmpCeilingZ) {
        gTmpCeilingZ = pm_opentop;
    }

    if (pm_openbottom > gTmpFloorZ) {
        gTmpFloorZ = pm_openbottom;
    }

    if (pm_lowfloor < gTmpDropoffZ) {
        gTmpDropoffZ = pm_lowfloor;
    }

    return true;
}

bool PIT_CheckThing(mobj_t& thing) noexcept {
    if ((thing.flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE)) == 0) {
        return true;
    }

    const Fixed blockdist = thing.radius + gpTmpThing->radius;
    Fixed delta = thing.x - gTmpX;

    if (delta < 0) {
        delta = -delta;
    }

    if (delta >= blockdist) {
        return true;    // Didn't hit it
    }

    delta = thing.y - gTmpY;
    if (delta < 0) {
        delta = -delta;
    }

    if (delta >= blockdist) {
        return true;    // Didn't hit it
    }

    if (&thing == gpTmpThing) {
        return true;    // Don't clip against self
    }

    // Check for skulls slamming into things
    if ((gpTmpThing->flags & MF_SKULLFLY) != 0) {
        gpMoveThing = &thing;
        return false;   // Stop moving
    }

    // Missiles can hit other things
    if ((gpTmpThing->flags & MF_MISSILE) != 0) {
        // See if it went over / under
        if (gpTmpThing->z > thing.z + thing.height) {
            return true;    // Overhead
        }
        
        if (gpTmpThing->z + gpTmpThing->height < thing.z) {
            return true;    // Underneath
        }

        if (gpTmpThing->target->InfoPtr == thing.InfoPtr) { // Don't hit same species as originator
            if (&thing == gpTmpThing->target) {
                return true;
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
        gpMoveThing = &thing;
        return false;   // Don't traverse any more
    }

    // Check for special pickup
    if (((thing.flags & MF_SPECIAL) != 0) && ((gTmpFlags & MF_PICKUP) != 0)) {
        gpMoveThing = &thing;
        return true;
    }

    return ((thing.flags & MF_SOLID) == 0);
}
