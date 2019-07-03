#include "Move.h"

#include "Game/Data.h"
#include "Info.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"

bool        gTryMove2;      // Result from P_TryMove2
bool        gFloatOk;       // If true, move would be ok if within tmfloorz - tmceilingz
Fixed       gTmpFloorZ;     // Current floor z for P_TryMove2
Fixed       gTmpCeilingZ;   // Current ceiling z for P_TryMove2
mobj_t*     gMoveThing;     // Either a skull/missile target or a special pickup
line_t*     gBlockLine;     // Might be a door that can be opened

static Fixed            gOldX;
static Fixed            gOldY;
static Fixed            gTmpBBox[4];
static uint32_t         gTmpFlags;
static Fixed            gTmpDropoffZ;       // Lowest point contacted
static subsector_t*     gNewSubSec;         // Dest subsector

/*
===================
=
= P_TryMove2
=
= Attempt to move to a new position, crossing special lines unless MF_TELEPORT
= is set
=
===================
*/
void P_TryMove2()
{
    gTryMove2 = false;       // until proven otherwise
    gFloatOk = false;

    gOldX = gTmpThing->x;
    gOldY = gTmpThing->y;

    PM_CheckPosition();

    if (gCheckPosOnly) {
        gCheckPosOnly = false;
        return;
    }

    if (!gTryMove2)
        return;

    if ( !(gTmpThing->flags & MF_NOCLIP) ) {
        gTryMove2 = false;

        if (gTmpCeilingZ - gTmpFloorZ < gTmpThing->height)
            return;         // doesn't fit
        gFloatOk = true;
        if ( !(gTmpThing->flags&MF_TELEPORT)
            &&gTmpCeilingZ - gTmpThing->z < gTmpThing->height)
            return;         // mobj must lower itself to fit
        if ( !(gTmpThing->flags&MF_TELEPORT)
            && gTmpFloorZ - gTmpThing->z > 24*FRACUNIT )
            return;         // too big a step up
        if ( !(gTmpThing->flags&(MF_DROPOFF|MF_FLOAT))
            && gTmpFloorZ - gTmpDropoffZ > 24*FRACUNIT )
            return;         // don't stand over a dropoff
    }

    //
    // the move is ok, so link the thing into its new position
    //
    UnsetThingPosition(gTmpThing);

    gTmpThing->floorz = gTmpFloorZ;
    gTmpThing->ceilingz = gTmpCeilingZ;
    gTmpThing->x = gTmpX;
    gTmpThing->y = gTmpY;

    SetThingPosition(gTmpThing);

    gTryMove2 = true;

    return;
}

static uint32_t PM_CrossCheck(line_t* ld) 
{
    if (PM_BoxCrossLine (ld))   {
        if (!PIT_CheckLine(ld)) {
            return false;
        }
    }
    return true;
}

/*
==================
=
= PM_CheckPosition
=
= This is purely informative, nothing is modified (except things picked up)

in:
tmthing     a mobj_t (can be valid or invalid)
tmx,tmy     a position to be checked (doesn't need relate to the mobj_t->x,y)

out:

newsubsec
floorz
ceilingz
tmdropoffz      the lowest point contacted (monsters won't move to a dropoff)
movething

==================
*/
void PM_CheckPosition() {
    gTmpFlags = gTmpThing->flags;
    
    gTmpBBox[BOXTOP] = gTmpY + gTmpThing->radius;
    gTmpBBox[BOXBOTTOM] = gTmpY - gTmpThing->radius;
    gTmpBBox[BOXRIGHT] = gTmpX + gTmpThing->radius;
    gTmpBBox[BOXLEFT] = gTmpX - gTmpThing->radius;
    
    gNewSubSec = PointInSubsector(gTmpX,gTmpY);

    // The base floor / ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    gTmpFloorZ = gTmpDropoffZ = gNewSubSec->sector->floorheight;
    gTmpCeilingZ = gNewSubSec->sector->ceilingheight;

    ++gValidCount;
    
    gMoveThing = 0;
    gBlockLine = 0;

    if (gTmpFlags & MF_NOCLIP) {
        gTryMove2 = true;
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

    for (int bx = xl; bx <= xh; bx++) {
        for (int by = yl; by <= yh; by++) {
            if (!BlockThingsIterator(bx,by,PIT_CheckThing)) {
                gTryMove2 = false;
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

    for (int bx = xl; bx <= xh; bx++) {
        for (int by = yl; by <= yh; by++) {
            if (!BlockLinesIterator(bx, by, PM_CrossCheck)) {
                gTryMove2 = false;
                return;
            }
        }
    }

    gTryMove2 = true;
    return;
}

//=============================================================================


/*
=================
=
= PM_BoxCrossLine
=
=================
*/
bool PM_BoxCrossLine(line_t* ld)
{
    Fixed       x1, y1, x2, y2;
    Fixed       lx, ly;
    Fixed       ldx, ldy;
    Fixed       dx1,dy1;
    Fixed       dx2,dy2;
    bool        side1, side2;

    if (gTmpBBox[BOXRIGHT] <= ld->bbox[BOXLEFT]
    ||  gTmpBBox[BOXLEFT] >= ld->bbox[BOXRIGHT]
    ||  gTmpBBox[BOXTOP] <= ld->bbox[BOXBOTTOM]
    ||  gTmpBBox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
        return false;

    y1 = gTmpBBox[BOXTOP];
    y2 = gTmpBBox[BOXBOTTOM];

    if (ld->slopetype == ST_POSITIVE) {
        x1 = gTmpBBox[BOXLEFT];
        x2 = gTmpBBox[BOXRIGHT];
    } else {
        x1 = gTmpBBox[BOXRIGHT];
        x2 = gTmpBBox[BOXLEFT];
    }

    lx = ld->v1.x;
    ly = ld->v1.y;
    ldx = (ld->v2.x-lx)>>16;
    ldy = (ld->v2.y-ly)>>16;

    dx1 = (x1 - lx)>>16;
    dy1 = (y1 - ly)>>16;
    dx2 = (x2 - lx)>>16;
    dy2 = (y2 - ly)>>16;

    side1 = ldy*dx1 < dy1*ldx;
    side2 = ldy*dx2 < dy2*ldx;

    return side1 != side2;
}

//=============================================================================


/*
==================
=
= PIT_CheckLine
=
= Adjusts tmfloorz and tmceilingz as lines are contacted
==================
*/
bool PIT_CheckLine(line_t* ld)
{
    Fixed       pm_opentop, pm_openbottom;
    Fixed       pm_lowfloor;
    sector_t    *front, *back;

    // a line has been hit

    /*
    =
    = The moving thing's destination position will cross the given line.
    = If this should not be allowed, return false.
    */
    if (!ld->backsector)
        return false;       // one sided line

    if (!(gTmpThing->flags & MF_MISSILE) )
    {
        if ( ld->flags & ML_BLOCKING )
            return false;       // explicitly blocking everything
        if ( !gTmpThing->player && ld->flags & ML_BLOCKMONSTERS )
            return false;       // block monsters only
    }


    front = ld->frontsector;
    back = ld->backsector;

    if (front->ceilingheight == front->floorheight
    || back->ceilingheight == back->floorheight)
    {
        gBlockLine = ld;
        return false;           // probably a closed door
    }

    if (front->ceilingheight < back->ceilingheight)
        pm_opentop = front->ceilingheight;
    else
        pm_opentop = back->ceilingheight;
    if (front->floorheight > back->floorheight)
    {
        pm_openbottom = front->floorheight;
        pm_lowfloor = back->floorheight;
    }
    else
    {
        pm_openbottom = back->floorheight;
        pm_lowfloor = front->floorheight;
    }

    // adjust floor / ceiling heights
    if (pm_opentop < gTmpCeilingZ)
        gTmpCeilingZ = pm_opentop;
    if (pm_openbottom > gTmpFloorZ)
        gTmpFloorZ = pm_openbottom;
    if (pm_lowfloor < gTmpDropoffZ)
        gTmpDropoffZ = pm_lowfloor;

    return true;
}

/*
==================
=
= PIT_CheckThing
=
==================
*/
uint32_t PIT_CheckThing(mobj_t* thing)
{
    Fixed       blockdist;
    int         delta;

    if (!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE) ))
        return true;
    blockdist = thing->radius + gTmpThing->radius;

    delta = thing->x - gTmpX;
    if (delta < 0)
        delta = -delta;
    if (delta >= blockdist)
        return true;        // didn't hit it
    delta = thing->y - gTmpY;
    if (delta < 0)
        delta = -delta;
    if (delta >= blockdist)
        return true;        // didn't hit it

    if (thing == gTmpThing)
        return true;        // don't clip against self

    //
    // check for skulls slamming into things
    //
    if (gTmpThing->flags & MF_SKULLFLY)
    {
        gMoveThing = thing;
        return false;       // stop moving
    }
    
    //
    // missiles can hit other things
    //
    if (gTmpThing->flags & MF_MISSILE)
    {
    // see if it went over / under
        if (gTmpThing->z > thing->z + thing->height)
            return true;        // overhead
        if (gTmpThing->z+gTmpThing->height < thing->z)
            return true;        // underneath
        if (gTmpThing->target->InfoPtr == thing->InfoPtr)
        {       // don't hit same species as originator
            if (thing == gTmpThing->target)
                return true;
            if (thing->InfoPtr != &gMObjInfo[MT_PLAYER])
                return false;   // explode, but do no damage
            // let players missile other players
        }
        if (! (thing->flags & MF_SHOOTABLE) )
            return !(thing->flags & MF_SOLID);      // didn't do any damage

    // damage / explode
        gMoveThing = thing;
        return false;           // don't traverse any more
    }

    //
    // check for special pickup
    //
    if ( (thing->flags&MF_SPECIAL) && (gTmpFlags&MF_PICKUP) )
    {
        gMoveThing = thing;
        return true;
    }

    return !(thing->flags & MF_SOLID);
}
