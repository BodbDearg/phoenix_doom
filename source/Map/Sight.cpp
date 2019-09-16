#include "Sight.h"

#include "Game/Data.h"
#include "MapData.h"
#include "MapUtil.h"
#include "Things/MapObj.h"

static Fixed        gSightZStart;       // Eye z of looker
static Fixed        gTopSlope;
static Fixed        gBottomSlope;       // Slopes to top and bottom of target
static vector_t     gSTrace;            // From t1 to t2
static Fixed        gT2x;
static Fixed        gT2y;
static int32_t      gT1xs;
static int32_t      gT1ys;
static int32_t      gT2xs;
static int32_t      gT2ys;

//---------------------------------------------------------------------------------------------------------------------
// First checks the endpoints of the line to make sure that they cross the sight trace
// treated as an infinite line.
//
// If so, it calculates the fractional distance along the sight trace that the intersection occurs at.
// If 0 < intercept < 1.0, the line will block the sight.
//---------------------------------------------------------------------------------------------------------------------
static Fixed PS_SightCrossLine(line_t& line) noexcept {
    // p1, p2 are line endpoints
    const int32_t p1x = line.v1.x >> 16;
    const int32_t p1y = line.v1.y >> 16;
    const int32_t p2x = line.v2.x >> 16;
    const int32_t p2y = line.v2.y >> 16;

    // p3, p4 are sight endpoints
    const int32_t p3x = gT1xs;
    const int32_t p3y = gT1ys;
    const int32_t p4x = gT2xs;
    const int32_t p4y = gT2ys;

    int32_t dx = p2x - p3x;
    int32_t dy = p2y - p3y;

    int32_t ndx = p4x - p3x;    // This can be precomputed if worthwhile
    int32_t ndy = p4y - p3y;

    bool sb1 = ((ndy * dx) < (dy * ndx));
    
    dx = p1x - p3x;
    dy = p1y - p3y;

    bool sb2 = ((ndy * dx) < (dy * ndx));

    if (sb1 == sb2) {
        return -1;  // Line isn't crossed
    }

    ndx = p1y - p2y;    // Vector normal to world line
    ndy = p2x - p1x;

    int32_t s1 = ndx * dx + ndy * dy;   // Distance projected onto normal

    dx = p4x - p1x;
    dy = p4y - p1y;

    int32_t s2 = ndx * dx + ndy * dy;   // Distance projected onto normal
    s2 = fixed16Div(s1, s1 + s2);

    return s2;
}

//---------------------------------------------------------------------------------------------------------------------
// Returns true if strace crosses the given subsector successfuly
//---------------------------------------------------------------------------------------------------------------------
static bool PS_CrossSubsector(const subsector_t& sub) noexcept {
    // Check lines
    seg_t* pSeg = sub.firstline;

    for (uint32_t count = sub.numsublines; count > 0; pSeg++, count--) {
        ASSERT(pSeg->linedef);
        line_t& line = *pSeg->linedef;

        if (line.validCount == gValidCount) {
            continue;   // Allready checked other side
        }

        line.validCount = gValidCount;
        Fixed frac = PS_SightCrossLine(line);

        if (frac < 4 || frac > FRACUNIT) {
            continue;
        }

        // Crosses line
        sector_t* const pBack = line.backsector;
        if (!pBack) {
            return false;   // One sided line
        }

        sector_t* const pFront = line.frontsector;

        if ((pFront->floorheight == pBack->floorheight) &&
            (pFront->ceilingheight == pBack->ceilingheight)
        ) {
            continue;   // No wall to block sight with
        }

        Fixed opentop;

        if (pFront->ceilingheight < pBack->ceilingheight) {
            opentop = pFront->ceilingheight;
        } else {
            opentop = pBack->ceilingheight;
        }

        Fixed openbottom;

        if (pFront->floorheight > pBack->floorheight) {
            openbottom = pFront->floorheight;
        } else {
            openbottom = pBack->floorheight;
        }

        // Quick test for totally closed doors
        if (openbottom >= opentop) {
            return false;   // Stop
        }

        frac >>= 2;

        if (pFront->floorheight != pBack->floorheight) {
            const Fixed slope = (((openbottom - gSightZStart) << 6) / frac) << 8;
            if (slope > gBottomSlope) {
                gBottomSlope = slope;
            }
        }

        if (pFront->ceilingheight != pBack->ceilingheight) {
            const Fixed slope = (((opentop - gSightZStart) << 6) / frac) << 8;
            if (slope < gTopSlope) {
                gTopSlope = slope;
            }
        }

        if (gTopSlope <= gBottomSlope) {
            return false;   // Stop
        }
    }

    return true;    // Passed the subsector ok
}

//---------------------------------------------------------------------------------------------------------------------
// Returns true if strace crosses the given node successfuly
//---------------------------------------------------------------------------------------------------------------------
static bool PS_CrossBSPNode(const node_t* const pNode) noexcept {
    if (isBspNodeASubSector(pNode)) {
        // N.B: pointer has to be fixed up due to prescence of a flag in the lowest bit!
        const subsector_t* const pSubSector = (const subsector_t*) getActualBspNodePtr(pNode);
        return PS_CrossSubsector(*pSubSector);
    }

    // Decide which side the start point is on
    const bool side = PointOnVectorSide(gSTrace.x, gSTrace.y, pNode->Line);

    // Cross the starting side
    if (!PS_CrossBSPNode((const node_t*) pNode->Children[side]))
        return false;
    
    // The partition plane is crossed here
    if (side == PointOnVectorSide(gT2x, gT2y, pNode->Line))
        return true;    // The line doesn't touch the other side
    
    // Cross the ending side
    return PS_CrossBSPNode((const node_t*) pNode->Children[side ^ 1]);
}

//---------------------------------------------------------------------------------------------------------------------
// Returns true if a straight line between t1 and t2 is unobstructed
//---------------------------------------------------------------------------------------------------------------------
bool CheckSight(mobj_t& t1, mobj_t& t2) noexcept {
    // Check for trivial rejection
    const uint32_t s1 = (uint32_t)(t1.subsector->sector - gpSectors);
    const uint32_t s2 = (uint32_t)(t2.subsector->sector - gpSectors);
    const uint32_t pnum = s1 * gNumSectors + s2;
    const uint32_t bytenum = pnum >> 3;
    const uint32_t bitnum = 1 << (pnum & 7);

    if ((gpRejectMatrix[bytenum] & bitnum) != 0) {
        return false;   // Can't possibly be connected
    }

    // Look from eyes of t1 to any part of t2
    ++gValidCount;

    // Make sure it never lies exactly on a vertex coordinate
    gSTrace.x = (t1.x & ~0x1ffff) | 0x10000;
    gSTrace.y = (t1.y & ~0x1ffff) | 0x10000;
    gT2x = (t2.x & ~0x1ffff) | 0x10000;
    gT2y = (t2.y & ~0x1ffff) | 0x10000;
    gSTrace.dx = gT2x - gSTrace.x;
    gSTrace.dy = gT2y - gSTrace.y;

    gT1xs = gSTrace.x >> 16;
    gT1ys = gSTrace.y >> 16;
    gT2xs = gT2x >> 16;
    gT2ys = gT2y >> 16;

    gSightZStart = t1.z + t1.height - (t1.height >> 2);
    gTopSlope = t2.z + t2.height - gSightZStart;
    gBottomSlope = t2.z - gSightZStart;

    return PS_CrossBSPNode(gpBSPTreeRoot);
}
