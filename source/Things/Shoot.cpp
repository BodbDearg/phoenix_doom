#include "Shoot.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"

//===================
//
// IN
//
// A line will be shootdivd from the middle of shooter in the direction of
// attackangle until either a shootable mobj is within the visible
// aimtopslope / aimbottomslope range, or a solid wall blocks further
// tracing.  If no thing is targeted along the entire range, the first line
// that blocks the midpoint of the shootdiv will be hit.
//===================

line_t*     gShootLine;
mobj_t*     gShootMObj;
Fixed       gShootSlope;        // between aimtop and aimbottom
Fixed       gShootX;
Fixed       gShootY;
Fixed       gShootZ;            // location for puff/blood

//===================
//
// TEMPS
//
//===================

static Fixed        gAimMidSlope;               // for detecting first wall hit
static vector_t     gShootDiv;
static Fixed        gShootX2;
static Fixed        gShootY2;
static Fixed        gFirstLineFrac;
static int          gShootDivPositive;
static Fixed        gOldFrac;
static void*        gOldValue;
static bool         gOldIsLine;
static int          gSsx1;
static int          gSsy1;
static int          gSsx2;
static int          gSsy2;

//---------------------------------------------------------------------------------------------------------------------
// Returns true if strace crosses the given node successfuly
//---------------------------------------------------------------------------------------------------------------------
static bool PA_CrossBSPNode(const node_t* pNode) {
    if (isBspNodeASubSector(pNode)) {
        // N.B: pointer has to be fixed up due to prescence of a flag in the lowest bit!
        const subsector_t* const pSubSector = reinterpret_cast<const subsector_t*>(getActualBspNodePtr(pNode));
        return PA_CrossSubsector(pSubSector);
    }
    
    // Decide which side the start point is on and cross the starting side
    const uint32_t side = PointOnVectorSide(gShootDiv.x, gShootDiv.y, &pNode->Line);
    
    if (!PA_CrossBSPNode((const node_t*) pNode->Children[side])) {
        return false;
    }

    // The partition plane is crossed here
    if (side == PointOnVectorSide(gShootX2, gShootY2, &pNode->Line)) {
        return true;    // The line doesn't touch the other side
    }
    
    // Cross the ending side
    return PA_CrossBSPNode((const node_t*) pNode->Children[side ^ 1]);
}

/*
=====================
=
= P_Shoot2
=
=====================
*/
void P_Shoot2()
{
    mobj_t      *t1;
    unsigned    angle;

    t1 = gShooter;

    gShootLine = 0;
    gShootMObj = 0;

    angle = gAttackAngle >> ANGLETOFINESHIFT;

    gShootDiv.x = t1->x;
    gShootDiv.y = t1->y;
    gShootX2 = t1->x + (gAttackRange >> FRACBITS) * gFineCosine[angle];
    gShootY2 = t1->y + (gAttackRange >> FRACBITS) * gFineSine[angle];
    gShootDiv.dx = gShootX2 - gShootDiv.x;
    gShootDiv.dy = gShootY2 - gShootDiv.y;
    gShootZ = t1->z + (t1->height>>1) + 8 * FRACUNIT;

    gShootDivPositive = (gShootDiv.dx ^ gShootDiv.dy)>0;

    gSsx1 = gShootDiv.x >> 16;
    gSsy1 = gShootDiv.y >> 16;
    gSsx2 = gShootX2 >> 16;
    gSsy2 = gShootY2 >> 16;

    gAimMidSlope = (gAimTopSlope + gAimBottomSlope)>>1;

    // Cross everything
    gOldFrac = 0;
    PA_CrossBSPNode(gpBSPTreeRoot);

    // Check the last intercept if needed
    if (!gShootMObj) {
        PA_DoIntercept(0, false, FRACUNIT);
    }

    // post process
    if (gShootMObj)
        return;

    if (!gShootLine)
        return;
    
    // Calculate the intercept point for the first line hit
    //
    // position a bit closer
    gFirstLineFrac -= fixedDiv(4 * FRACUNIT, gAttackRange);

    gShootX = gShootDiv.x + fixedMul(gShootDiv.dx, gFirstLineFrac);
    gShootY = gShootDiv.y + fixedMul(gShootDiv.dy, gFirstLineFrac);
    gShootZ = gShootZ + fixedMul(gAimMidSlope, fixedMul(gFirstLineFrac, gAttackRange));
}


/*
==================
=
= PA_DoIntercept
=
==================
*/
bool PA_DoIntercept(void *value, bool isline, Fixed frac) {
    intptr_t temp;

    if (gOldFrac < frac)
    {
        temp = (intptr_t) gOldValue;
        gOldValue = value;
        value = (void *)temp;

        temp = gOldIsLine;
        gOldIsLine = isline;
        isline = temp;

        temp = gOldFrac;
        gOldFrac = frac;
        frac = temp;
    }

    if (frac == 0 || frac >= FRACUNIT)
        return true;

    if (isline)
        return PA_ShootLine ((line_t *)value, frac);

    return PA_ShootThing ((mobj_t *)value, frac);
}


/*
==============================================================================
=
= PA_ShootLine
=
==============================================================================
*/
bool PA_ShootLine(line_t* li, Fixed interceptfrac)
{
    Fixed       slope;
    Fixed       dist;
    sector_t    *front, *back;
    Fixed opentop,openbottom;

    if ( !(li->flags & ML_TWOSIDED) )
    {
        if (!gShootLine)
        {
            gShootLine = li;
            gFirstLineFrac = interceptfrac;
        }
        gOldFrac = 0;   // don't shoot anything past this
        return false;
    }

//
// crosses a two sided line
//
    front = li->frontsector;
    back = li->backsector;

    if (front->ceilingheight < back->ceilingheight) {
        opentop = front->ceilingheight;
    }
    else {
        opentop = back->ceilingheight;
    }

    if (front->floorheight > back->floorheight) {
        openbottom = front->floorheight;
    }
    else {
        openbottom = back->floorheight;
    }

    dist = fixedMul(gAttackRange, interceptfrac);

    if (li->frontsector->floorheight != li->backsector->floorheight) {
        slope = fixedDiv(openbottom - gShootZ, dist);

        if (slope >= gAimMidSlope && !gShootLine) {
            gShootLine = li;
            gFirstLineFrac = interceptfrac;
        }

        if (slope > gAimBottomSlope) {
            gAimBottomSlope = slope;
        }
    }

    if (li->frontsector->ceilingheight != li->backsector->ceilingheight) {
        slope = fixedDiv(opentop - gShootZ, dist);

        if (slope <= gAimMidSlope && !gShootLine) {
            gShootLine = li;
            gFirstLineFrac = interceptfrac;
        }

        if (slope < gAimTopSlope) {
            gAimTopSlope = slope;
        }
    }

    if (gAimTopSlope <= gAimBottomSlope)
        return false;       // stop
    
    return true;        // shot continues
}

/*
==============================================================================
=
= PA_ShootThing
=
==============================================================================
*/
bool PA_ShootThing(mobj_t* th, Fixed interceptfrac) {
    Fixed       frac;
    Fixed       dist;
    Fixed       thingaimtopslope, thingaimbottomslope;

    if (th == gShooter)
        return true;        // can't shoot self
    if (!(th->flags&MF_SHOOTABLE))
        return true;        // corpse or something

// check angles to see if the thing can be aimed at
    dist = fixedMul(gAttackRange, interceptfrac);
    thingaimtopslope = fixedDiv(th->z + th->height - gShootZ, dist);

    if (thingaimtopslope < gAimBottomSlope)
        return true;        // shot over the thing

    thingaimbottomslope = fixedDiv(th->z - gShootZ, dist);

    if (thingaimbottomslope > gAimTopSlope)
        return true;        // shot under the thing

//
// this thing can be hit!
//
    if (thingaimtopslope > gAimTopSlope)
        thingaimtopslope = gAimTopSlope;
    if (thingaimbottomslope < gAimBottomSlope)
        thingaimbottomslope = gAimBottomSlope;

    // shoot midway in the visible part of the thing
    gShootSlope = (thingaimtopslope+thingaimbottomslope)/2;
    gShootMObj = th;

    // position a bit closer
    frac = interceptfrac - fixedDiv(10 * FRACUNIT, gAttackRange);
    gShootX = gShootDiv.x + fixedMul(gShootDiv.dx, frac);
    gShootY = gShootDiv.y + fixedMul(gShootDiv.dy, frac);
    gShootZ = gShootZ + fixedMul(gShootSlope, fixedMul(frac, gAttackRange));

    return false;   // Don't go any farther
}


/*
=================
=
= PA_SightCrossLine
=
= First checks the endpoints of the line to make sure that they cross the
= sight trace treated as an infinite line.
=
= If so, it calculates the fractional distance along the sight trace that
= the intersection occurs at.  If 0 < intercept < 1.0, the line will block
= the sight.
=================
*/
Fixed PA_SightCrossLine(line_t* line) {
    int         s1, s2;
    int         p1x,p1y,p2x,p2y,p3x,p3y,p4x,p4y,dx,dy,ndx,ndy;

// p1, p2 are line endpoints
    p1x = line->v1.x >> 16;
    p1y = line->v1.y >> 16;
    p2x = line->v2.x >> 16;
    p2y = line->v2.y >> 16;

// p3, p4 are sight endpoints
    p3x = gSsx1;
    p3y = gSsy1;
    p4x = gSsx2;
    p4y = gSsy2;

    dx = p2x - p3x;
    dy = p2y - p3y;

    ndx = p4x - p3x;        // this can be precomputed if worthwhile
    ndy = p4y - p3y;

    s1 =  (ndy * dx) <  (dy * ndx);

    dx = p1x - p3x;
    dy = p1y - p3y;

    s2 =  (ndy * dx) <  (dy * ndx);

    if (s1 == s2)
        return -1;          // line isn't crossed

    ndx = p1y - p2y;        // vector normal to world line
    ndy = p2x - p1x;

    s1 = ndx*dx + ndy*dy;   // distance projected onto normal

    dx = p4x - p1x;
    dy = p4y - p1y;

    s2 = ndx*dx + ndy*dy;   // distance projected onto normal

    s2 = fixedDiv(s1, s1 + s2);

    return s2;
}



/*
=================
=
= PA_CrossSubsectorPass
=
= Returns true if strace crosses the given subsector successfuly
=================
*/

static struct {
 vertex_t tv1,tv2;
} thingline;
bool PA_CrossSubsector(const subsector_t* sub) {
    seg_t       *seg;
    line_t      *line;
    int         count;
    Fixed       frac;
    mobj_t      *thing;

//
// check things
//
    for (thing = sub->sector->thinglist ; thing ; thing = thing->snext )
    {
        if (thing->subsector != sub)
            continue;

        // check a corner to corner crossection for hit

        if (gShootDivPositive) {
            thingline.tv1.x = thing->x - thing->radius;
            thingline.tv1.y = thing->y + thing->radius;

            thingline.tv2.x = thing->x + thing->radius;
            thingline.tv2.y = thing->y - thing->radius;
        } else {
            thingline.tv1.x = thing->x - thing->radius;
            thingline.tv1.y = thing->y - thing->radius;

            thingline.tv2.x = thing->x + thing->radius;
            thingline.tv2.y = thing->y + thing->radius;
        }

        frac = PA_SightCrossLine ((line_t *)&thingline);

        if (frac < 0 || frac > FRACUNIT) {
            continue;
        }
        if (!PA_DoIntercept ( (void *)thing, false, frac)) {
            return false;
        }
    }

//
// check lines
//
    count = sub->numsublines;
    seg = sub->firstline;

    for ( ; count ; seg++, count--)
    {
        line = seg->linedef;

        if (line->validcount == gValidCount)
            continue;       // already checked other side
        line->validcount = gValidCount;

        frac = PA_SightCrossLine (line);

        if (frac < 0 || frac > FRACUNIT)
            continue;

        if (!PA_DoIntercept ( (void *)line, true, frac))
//      if (!PA_ShootLine (line, frac))
            return false;
    }


    return true;            // passed the subsector ok
}
