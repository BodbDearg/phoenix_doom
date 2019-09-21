#include "Shoot.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"
#include <algorithm>
#include <vector>

BEGIN_NAMESPACE(Shoot)

//----------------------------------------------------------------------------------------------------------------------
// Input values:
//
//  A line will be shootdivd from the middle of shooter in the direction of attackangle until either a shootable
//  mobj is within the visible aimtopslope / aimbottomslope range, or a solid wall blocks further tracing.
//  If no thing is targeted along the entire range, the first line that blocks the midpoint of the shootdiv will be hit.
//----------------------------------------------------------------------------------------------------------------------
line_t*     gpShootLine;
mobj_t*     gpShootMObj;
Fixed       gShootSlope;    // Between aimtop and aimbottom
Fixed       gShootX;
Fixed       gShootY;
Fixed       gShootZ;        // Location for puff/blood

//----------------------------------------------------------------------------------------------------------------------
// Represents a possible hit with something being shit.
// Stores the hit fraction and the thing hit, and whether it is a thing or a line.
// Also supports sorting, so we process the closest hit first!
//----------------------------------------------------------------------------------------------------------------------
struct Intercept {
    Fixed   frac;
    void*   pObj;
    bool    bIsLine;

    inline bool operator < (const Intercept& other) noexcept {
        return (frac < other.frac);
    }
};

static std::vector<Intercept>   gIntercepts;
static Fixed                    gAimMidSlope;           // For detecting first wall hit
static vector_t                 gShootDiv;
static Fixed                    gShootX2;
static Fixed                    gShootY2;
static Fixed                    gFirstLineFrac;
static bool                     gbShootDivPositive;
static Fixed                    gOldFrac;
static void*                    gpOldValue;
static bool                     gOldIsLine;
static int32_t                  gSsx1;
static int32_t                  gSsy1;
static int32_t                  gSsx2;
static int32_t                  gSsy2;

//----------------------------------------------------------------------------------------------------------------------
// Returns true if strace crosses the given node successfuly
//----------------------------------------------------------------------------------------------------------------------
static bool PA_CrossBSPNode(node_t* pNode) {
    if (isBspNodeASubSector(pNode)) {
        // N.B: pointer has to be fixed up due to prescence of a flag in the lowest bit!
        const subsector_t* const pSubSector = (subsector_t*) getActualBspNodePtr(pNode);
        return PA_CrossSubsector(*pSubSector);
    }

    // Decide which side the start point is on and cross the starting side
    const bool bOnRightSide = PointOnVectorSide(gShootDiv.x, gShootDiv.y, pNode->Line);
    const uint32_t sideIdx = (bOnRightSide) ? 1 : 0;

    if (!PA_CrossBSPNode((node_t*) pNode->Children[sideIdx])) {
        return false;
    }

    // The partition plane is crossed here
    if (bOnRightSide == PointOnVectorSide(gShootX2, gShootY2, pNode->Line)) {
        return true;    // The line doesn't touch the other side
    }

    // Cross the ending side
    return PA_CrossBSPNode((node_t*) pNode->Children[sideIdx ^ 1]);
}

static bool PA_DoIntercept(void* pValue, bool isLine, Fixed frac) noexcept {
    if (gOldFrac < frac) {
        std::swap(gpOldValue, pValue);
        std::swap(gOldIsLine, isLine);
        std::swap(gOldFrac, frac);
    }

    if (frac == 0 || frac >= FRACUNIT)
        return true;

    ASSERT(pValue);
    
    if (isLine) {
        line_t* const pLine = (line_t*) pValue;
        return PA_ShootLine(*pLine, frac);
    } else {
        mobj_t* const pThing = (mobj_t*) pValue;
        return PA_ShootThing(*pThing, frac);
    }
}

void init() noexcept {
    gIntercepts.reserve(64);
}

void shutdown() noexcept {
    gIntercepts.clear();
    gIntercepts.shrink_to_fit();
}

void P_Shoot2() noexcept {
    ASSERT(gpShooter);
    mobj_t& shooter = *gpShooter;

    gpShootLine = nullptr;
    gpShootMObj = nullptr;

    const uint32_t angle = gAttackAngle >> ANGLETOFINESHIFT;

    gShootDiv.x = shooter.x;
    gShootDiv.y = shooter.y;
    gShootX2 = shooter.x + (gAttackRange >> FRACBITS) * gFineCosine[angle];
    gShootY2 = shooter.y + (gAttackRange >> FRACBITS) * gFineSine[angle];
    gShootDiv.dx = gShootX2 - gShootDiv.x;
    gShootDiv.dy = gShootY2 - gShootDiv.y;
    gShootZ = shooter.z + (shooter.height >> 1) + 8 * FRACUNIT;

    gbShootDivPositive = ((gShootDiv.dx ^ gShootDiv.dy) > 0);

    gSsx1 = gShootDiv.x >> 16;
    gSsy1 = gShootDiv.y >> 16;
    gSsx2 = gShootX2 >> 16;
    gSsy2 = gShootY2 >> 16;

    gAimMidSlope = (gAimTopSlope + gAimBottomSlope) >> 1;

    // Cross everything
    gOldFrac = 0;
    PA_CrossBSPNode(gpBSPTreeRoot);

    // Check the last intercept if needed
    if (!gpShootMObj) {
        PA_DoIntercept(nullptr, false, FRACUNIT);
    }

    // post process
    if (gpShootMObj)
        return;

    if (!gpShootLine)
        return;

    // Calculate the intercept point for the first line hit. Position a bit closer:
    gFirstLineFrac -= fixed16Div(4 * FRACUNIT, gAttackRange);

    gShootX = gShootDiv.x + fixed16Mul(gShootDiv.dx, gFirstLineFrac);
    gShootY = gShootDiv.y + fixed16Mul(gShootDiv.dy, gFirstLineFrac);
    gShootZ = gShootZ + fixed16Mul(gAimMidSlope, fixed16Mul(gFirstLineFrac, gAttackRange));
}

bool PA_ShootLine(line_t& li, const Fixed interceptfrac) noexcept {
    if ((li.flags & ML_TWOSIDED) == 0) {
        if (!gpShootLine) {
            gpShootLine = &li;
            gFirstLineFrac = interceptfrac;
        }

        gOldFrac = 0;   // Don't shoot anything past this
        return false;
    }

    // Crosses a two sided line make sure the line has a gap that can be shot through first
    sector_t& frontSec = *li.frontsector;
    sector_t& backSec = *li.backsector;

    {
        Fixed maxFloor = std::max(frontSec.floorheight, backSec.floorheight);
        Fixed minCeil = std::min(frontSec.ceilingheight, backSec.ceilingheight);

        if (maxFloor >= minCeil) {
            // No gap/opening: don't shoot anything past this
            if (!gpShootLine) {
                gpShootLine = &li;
                gFirstLineFrac = interceptfrac;
            }

            gOldFrac = 0;
            return false;
        }
    }
    
    // Do the rest of the shooting logic
    Fixed opentop;
    Fixed openbottom;

    if (frontSec.ceilingheight < backSec.ceilingheight) {
        opentop = frontSec.ceilingheight;
    } else {
        opentop = backSec.ceilingheight;
    }

    if (frontSec.floorheight > backSec.floorheight) {
        openbottom = frontSec.floorheight;
    } else {
        openbottom = backSec.floorheight;
    }

    const Fixed dist = fixed16Mul(gAttackRange, interceptfrac);

    if (li.frontsector->floorheight != li.backsector->floorheight) {
        const Fixed slope = fixed16Div(openbottom - gShootZ, dist);

        if ((slope >= gAimMidSlope) && (!gpShootLine)) {
            gpShootLine = &li;
            gFirstLineFrac = interceptfrac;
        }

        if (slope > gAimBottomSlope) {
            gAimBottomSlope = slope;
        }
    }

    if (li.frontsector->ceilingheight != li.backsector->ceilingheight) {
        const Fixed slope = fixed16Div(opentop - gShootZ, dist);

        if ((slope <= gAimMidSlope) && (!gpShootLine)) {
            gpShootLine = &li;
            gFirstLineFrac = interceptfrac;
        }

        if (slope < gAimTopSlope) {
            gAimTopSlope = slope;
        }
    }

    if (gAimTopSlope <= gAimBottomSlope)
        return false;   // Stop

    return true;    // Shot continues
}

bool PA_ShootThing(mobj_t& th, const Fixed interceptfrac) noexcept {
    if (&th == gpShooter)
        return true;    // Can't shoot self

    if ((th.flags & MF_SHOOTABLE) == 0)
        return true;    // Corpse or something
    
    // Check angles to see if the thing can be aimed at
    const Fixed dist = fixed16Mul(gAttackRange, interceptfrac);
    Fixed thingaimtopslope = fixed16Div(th.z + th.height - gShootZ, dist);

    if (thingaimtopslope < gAimBottomSlope)
        return true;    // Shot over the thing

    Fixed thingaimbottomslope = fixed16Div(th.z - gShootZ, dist);

    if (thingaimbottomslope > gAimTopSlope)
        return true;    // Shot under the thing
    
    // This thing can be hit!
    if (thingaimtopslope > gAimTopSlope) {
        thingaimtopslope = gAimTopSlope;
    }

    if (thingaimbottomslope < gAimBottomSlope) {
        thingaimbottomslope = gAimBottomSlope;
    }

    // Shoot midway in the visible part of the thing
    gShootSlope = (thingaimtopslope + thingaimbottomslope) / 2;
    gpShootMObj = &th;

    // position a bit closer
    const Fixed frac = interceptfrac - fixed16Div(10 * FRACUNIT, gAttackRange);
    gShootX = gShootDiv.x + fixed16Mul(gShootDiv.dx, frac);
    gShootY = gShootDiv.y + fixed16Mul(gShootDiv.dy, frac);
    gShootZ = gShootZ + fixed16Mul(gShootSlope, fixed16Mul(frac, gAttackRange));

    return false;   // Don't go any farther
}

//----------------------------------------------------------------------------------------------------------------------
// First checks the endpoints of the line to make sure that they cross the sight trace treated as an infinite line.
// 
// If so, it calculates the fractional distance along the sight trace that the intersection occurs at.
// If 0 < intercept < 1.0, the line will block the sight.
//----------------------------------------------------------------------------------------------------------------------
Fixed PA_SightCrossLine(const vertex_t& lineV1, const vertex_t& lineV2) noexcept {
    // P1, P2 are line endpoints
    int32_t p1x = fixed16ToInt(lineV1.x);
    int32_t p1y = fixed16ToInt(lineV1.y);
    int32_t p2x = fixed16ToInt(lineV2.x);
    int32_t p2y = fixed16ToInt(lineV2.y);
    
    // P3, P4 are sight endpoints
    int32_t p3x = gSsx1;
    int32_t p3y = gSsy1;
    int32_t p4x = gSsx2;
    int32_t p4y = gSsy2;

    int32_t dx = p2x - p3x;
    int32_t dy = p2y - p3y;

    // This can be precomputed if worthwhile
    int32_t ndx = p4x - p3x;
    int32_t ndy = p4y - p3y;
    bool bs1 = (ndy * dx) < (dy * ndx);

    dx = p1x - p3x;
    dy = p1y - p3y;
    bool bs2 = (ndy * dx) < (dy * ndx);

    if (bs1 == bs2)
        return -1;  // line isn't crossed
    
    // Vector normal to world line
    ndx = p1y - p2y;
    ndy = p2x - p1x;
    int32_t s1 = ndx * dx + ndy * dy;   // Distance projected onto normal

    dx = p4x - p1x;
    dy = p4y - p1y;
    int32_t s2 = ndx * dx + ndy * dy;   // Distance projected onto normal

    s2 = fixed16Div(s1, s1 + s2);
    return s2;
}

Fixed PA_SightCrossLine(const line_t& line) noexcept {
    return PA_SightCrossLine(line.v1, line.v2);
}

//----------------------------------------------------------------------------------------------------------------------
// Returns true if strace crosses the given subsector successfuly
//----------------------------------------------------------------------------------------------------------------------
bool PA_CrossSubsector(const subsector_t& sub) noexcept {
    // Ensure this list is clear before we begin
    gIntercepts.clear();
    
    // Check lines
    {
        seg_t* pSeg = sub.firstline;

        for (uint32_t count = sub.numsublines; count > 0; pSeg++, count--) {
            line_t& line = *pSeg->linedef;

            if (line.validCount == gValidCount)
                continue;   // Already checked other side

            line.validCount = gValidCount;
            const Fixed frac = PA_SightCrossLine(line);

            if (frac <=  0 || frac > FRACUNIT)
                continue;

            Intercept intercept;
            intercept.frac = frac;
            intercept.bIsLine = true;
            intercept.pObj = &line;

            gIntercepts.push_back(intercept);
        }
    }

    // Check things in the sector
    for (mobj_t* pThing = sub.sector->thinglist; pThing != nullptr; pThing = pThing->snext) {
        if (pThing->subsector != &sub)
            continue;

        if ((pThing->flags & MF_SHOOTABLE) == 0)
            continue;
        
        // Check a corner to corner crossection for hit
        vertex_t thingLineV1;
        vertex_t thingLineV2;

        if (gbShootDivPositive) {
            thingLineV1.x = pThing->x - pThing->radius;
            thingLineV1.y = pThing->y + pThing->radius;
            thingLineV2.x = pThing->x + pThing->radius;
            thingLineV2.y = pThing->y - pThing->radius;
        } else {
            thingLineV1.x = pThing->x - pThing->radius;
            thingLineV1.y = pThing->y - pThing->radius;
            thingLineV2.x = pThing->x + pThing->radius;
            thingLineV2.y = pThing->y + pThing->radius;
        }

        const Fixed frac = PA_SightCrossLine(thingLineV1, thingLineV2);

        if (frac <= 0 || frac > FRACUNIT)
            continue;

        Intercept intercept;
        intercept.frac = frac;
        intercept.bIsLine = false;
        intercept.pObj = pThing;

        gIntercepts.push_back(intercept);
    }

    // Start processing intercepts, with the closest first
    std::sort(gIntercepts.begin(), gIntercepts.end());

    for (const Intercept& intercept : gIntercepts) {
        if (!PA_DoIntercept(intercept.pObj, intercept.bIsLine, intercept.frac))
            return false;
    }

    return true;            // Passed the subsector ok
}

END_NAMESPACE(Shoot)
