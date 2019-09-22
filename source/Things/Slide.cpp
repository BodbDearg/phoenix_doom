#include "Slide.h"

#include "Game/Data.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"
#include <algorithm>
#include <vector>

//----------------------------------------------------------------------------------------------------------------------
// Newly rewritten sliding logic for added movement smoothness.
// Note: this code could be optimized A LOT, there are many things that could be precomputed...
//
// I figured the overhead of this was so low though it probably isn't worth it given that it just runs for the player
// and only once a frame, colliding with a couple of things at most. Can revisit later though if required!
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Slide)

// Use this much of a bounding circle for sliding for walls and things.
// These are bigger than the actual Doom player radius so we don't 'stick' to things and get stuck.
static constexpr float CLIP_RADIUS_WALLS = 24.0f;
static constexpr float CLIP_RADIUS_THINGS = 28.0f;

// Don't bother resolving penetrations if they are this small or less
static constexpr float MIN_PENETRATION = 0.125f;

// Use this much of a bounding circle for deciding when to go into both sides of a BSP split.
// Making it bigger so we account for large sprites and so forth.
static constexpr float BSP_RADIUS = 64.0f;

Fixed       gSlideX;
Fixed       gSlideY;
line_t*     gpSpecialLine;

struct CollisionResponse {
    float moveX;
    float moveY;
};

static mobj_t*                          gpSlideThing;
static std::vector<CollisionResponse>   gCollisionResponses;

//----------------------------------------------------------------------------------------------------------------------
// Checks to see what side of a line a point is on
//----------------------------------------------------------------------------------------------------------------------
static bool SL_PointOnSide2(
    int32_t x1,
    int32_t y1,
    const int32_t x2,
    const int32_t y2,
    const int32_t x3,
    const int32_t y3
) noexcept {
    x1 = x1 - x2;
    y1 = y1 - y2;
    
    const int32_t nx = y3 - y2;
    const int32_t ny = x2 - x3;
    const Fixed dist = fixed16Mul(x1, nx) + fixed16Mul(y1, ny);

    if (dist < 0) {
        return false;   // Point at the back of the line
    } else {
        return true;    // Point in front of the line
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Checks to see if we have crossed any special trigger lines and sets the special line if so
//----------------------------------------------------------------------------------------------------------------------
static void SL_CheckSpecialLines(
    const Fixed x1,
    const Fixed y1,
    const Fixed x2,
    const Fixed y2
) noexcept {
    Fixed xl;
    Fixed xh;
    Fixed yl;
    Fixed yh;

    if (x1 < x2) {
        xl = x1;
        xh = x2;
    } else {
        xl = x2;
        xh = x1;
    }

    if (y1<y2) {
        yl = y1;
        yh = y2;
    } else {
        yl = y2;
        yh = y1;
    }

    int32_t bxl = (xl - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int32_t bxh = (xh - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int32_t byl = (yl - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    int32_t byh = (yh - gBlockMapOriginY) >> MAPBLOCKSHIFT;

    bxl = std::max(bxl, 0);
    byl = std::max(byl, 0);

    if (bxh < 0 || byh < 0)
        return;
    
    if (bxh >= (int32_t) gBlockMapWidth) {
        bxh = (int32_t) gBlockMapWidth - 1;
    }

    if (byh >= (int32_t) gBlockMapHeight) {
        byh = (int32_t) gBlockMapHeight - 1;
    }

    gpSpecialLine = nullptr;    
    ++gValidCount;

    line_t* gpLd = nullptr;

    for (uint32_t bx = (uint32_t) bxl; bx <= (uint32_t) bxh; bx++) {
        for (uint32_t by = (uint32_t) byl; by <= (uint32_t) byh; by++) {
            for (line_t** gppList = gpBlockMapLineLists[(by * gBlockMapWidth) + bx]; gppList[0]; ++gppList) {
                gpLd = gppList[0];

                if (gpLd->special <= 0) {
                    continue;
                }

                if (gpLd->validCount == gValidCount) {
                    continue;   // Line has already been checked
                }

                gpLd->validCount = gValidCount;

                if ((xh < gpLd->bbox[BOXLEFT]) ||
                    (xl > gpLd->bbox[BOXRIGHT]) ||
                    (yh < gpLd->bbox[BOXBOTTOM]) ||
                    (yl > gpLd->bbox[BOXTOP])
                ) {
                    continue;
                }

                const int32_t x3 = gpLd->v1.x;
                const int32_t y3 = gpLd->v1.y;
                const int32_t x4 = gpLd->v2.x;
                const int32_t y4 = gpLd->v2.y;

                uint32_t side1 = SL_PointOnSide2(x1, y1, x3, y3, x4, y4);
                uint32_t side2 = SL_PointOnSide2(x2, y2, x3, y3, x4, y4);

                if (side1 == side2) {
                    continue;   // Move doesn't cross line
                }

                side1 = SL_PointOnSide2(x3, y3, x1, y1, x2, y2);
                side2 = SL_PointOnSide2(x4, y4, x1, y1, x2, y2);

                if (side1 == side2) {
                    continue;   // Line doesn't cross move
                }

                gpSpecialLine = gpLd;
                return;
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Slide around a thing
//----------------------------------------------------------------------------------------------------------------------
void slideCollideWithThing(const mobj_t& thing) noexcept {
    // Don't collide with the slide thing
    if (&thing == gpSlideThing)
        return;
    
    // See if we ignore the thing for sliding against
    const uint32_t thingFlags = thing.flags;
    const bool bIgnoreDueToFlags = (
        ((thingFlags & MF_SOLID) == 0) ||           // Ignore things that aren't solid
        ((thingFlags & MF_COUNTKILL) != 0) ||       // Ignore monsters, they move around too much and can produce judder if we try to slide against them
        ((thingFlags & MF_MISSILE) != 0) ||         // Don't do this for missiles
        ((thingFlags & MF_SKULLFLY) != 0)           // Don't do this for flying skulls
    );

    if (bIgnoreDueToFlags)
        return;
    
    // Get a vector between the player and the thing and get the square length of that
    const float vx = fixed16ToFloat(gSlideX - thing.x);
    const float vy = fixed16ToFloat(gSlideY - thing.y);
    const float vSqLen = vx * vx + vy * vy;

    // Get the required separation length (in square units)
    const float reqSep = CLIP_RADIUS_THINGS + fixed16ToFloat(thing.radius);
    const float reqSqSep = reqSep * reqSep;

    // See if the player and the thing need to be separated
    const bool bCollisionWithThing = (vSqLen < reqSqSep);

    if (bCollisionWithThing) {
        // Need separation, figure out the penetration amount
        const float vLen = std::sqrt(vSqLen);
        const float penetration = std::max(reqSep - vLen, 0.0f);

        if (penetration >= MIN_PENETRATION) {
            // Normalize the vector between, then move out by the penetration amount in this dir
            const float vdx = vx / vLen;
            const float vdy = vy / vLen;

            CollisionResponse response;
            response.moveX = vdx * penetration;
            response.moveY = vdy * penetration;
            gCollisionResponses.push_back(response);
        }
    }
}

void slideCollideWithSectorThings(sector_t& sector) noexcept {
    if (sector.validcount != gValidCount) {
        sector.validcount = gValidCount;
        mobj_t* pThing = sector.thinglist;

        while (pThing) {
            slideCollideWithThing(*pThing);
            pThing = pThing->snext;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Tells if the seg should be collided with for the purposes of sliding
//----------------------------------------------------------------------------------------------------------------------
bool isSegCollidableDuringSlide(const seg_t& seg) noexcept {
    // If the line is 1 sided or blocking, then it's definitely collidable
    const line_t& line = *seg.linedef;

    if (!line.backsector)
        return true;
    
    if ((line.flags & ML_BLOCKING) != 0)
        return true;
    
    // See if this seg has too much of a step up for it to be passable
    constexpr Fixed MAX_STEP_UP = intToFixed16(24);

    const sector_t& fsec = *seg.frontsector;
    const sector_t& bsec = *seg.backsector;
    const Fixed stepUp = bsec.floorheight - fsec.floorheight;

    if (stepUp > MAX_STEP_UP) {
        // This is a large step up that could be blocking.
        // Only consider it blocking however if the player can't step up at current it's height:
        const Fixed playerZ = std::max(gpSlideThing->z, gpSlideThing->floorz);
        const Fixed playerStepUp = bsec.floorheight - playerZ;

        if (playerStepUp > MAX_STEP_UP)
            return true;
    }

    // See if the gap between sectors is too small to pass through.
    // Note: consider the player's z location as a 'floor' if that is higher than the actual floor.
    // The player may need to drop in order to pass through...
    constexpr Fixed MIN_SECTOR_GAP = intToFixed16(56);

    const Fixed maxFloor = std::max(std::max(fsec.floorheight, bsec.floorheight), gpSlideThing->z);
    const Fixed minCeil = std::min(fsec.ceilingheight, bsec.ceilingheight);
    const Fixed gapBetweenSectors = minCeil - maxFloor;

    if (gapBetweenSectors < MIN_SECTOR_GAP)
        return true;
    
    // If we get to here then the line is not collidable
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// Do sliding against a line segment
//----------------------------------------------------------------------------------------------------------------------
void slideCollideWithLineSeg(seg_t& seg) noexcept {
    // First check if the line is actually collidable and abort if not
    if (!isSegCollidableDuringSlide(seg))
        return;

    // Ensure the line wasn't already processed and mark it so we don't process again during this round
    line_t& line = *seg.linedef;

    if (line.validCount == gValidCount)
        return;
    
    line.validCount = gValidCount;
    
    // See if we are on the back side of this seg.
    // If that is the case then we can ignore the seg for the purposes of collision:
    const float slideX = fixed16ToFloat(gSlideX);
    const float slideY = fixed16ToFloat(gSlideY);

    {
        const float segDirX = seg.v2.x - seg.v1.x;
        const float segDirY = seg.v2.y - seg.v1.y;
        const float segNormX = segDirY;
        const float segNormY = -segDirX;
        const float slideRelX = slideX - seg.v1.x;
        const float slideRelY = slideY - seg.v1.y;

        if (slideRelX * segNormX + slideRelY * segNormY < 0.0f)
            return;
    }

    // Okay we are NOT on the back side of the seg.
    // Figure out our actual distance to the line plane and reject if too far away to it.
    // Note that we may have to reverse the line depending on what side of it we are on...
    float lineP1x, lineP1y;
    float lineP2x, lineP2y;

    if (seg.getLineSideIndex() == 0) {
        lineP1x = line.v1f.x;
        lineP1y = line.v1f.y;
        lineP2x = line.v2f.x;
        lineP2y = line.v2f.y;
    } else {
        lineP1x = line.v2f.x;
        lineP1y = line.v2f.y;
        lineP2x = line.v1f.x;
        lineP2y = line.v1f.y;
    }

    float lineDirX = lineP2x - lineP1x;
    float lineDirY = lineP2y - lineP1y;
    const float lineLen = std::sqrt(lineDirX * lineDirX + lineDirY * lineDirY);
    ASSERT(lineLen > 0);

    lineDirX /= lineLen;
    lineDirY /= lineLen;

    const float lineNormX = lineDirY;
    const float lineNormY = -lineDirX;
    const float slideV1RelX = slideX - lineP1x;
    const float slideV1RelY = slideY - lineP1y;
    const float distToLine = slideV1RelX * lineNormX + slideV1RelY * lineNormY;

    if (distToLine >= CLIP_RADIUS_WALLS)
        return;
    
    if (distToLine < 0)
        return;
    
    // Okay, close enough to the line plane.
    // See if we maybe collide with the first point, the line itself or the second point:
    const float distAlongLine = slideV1RelX * lineDirX + slideV1RelY * lineDirY;

    if (distAlongLine < 0.0f) {
        // Maybe colliding with the start point of the line: see how far away we are from it
        if (distAlongLine > -CLIP_RADIUS_WALLS) {
            // Colliding with the line start point.
            // Get the distance to it to determine the penetration, then move back along the normal.
            const float distToPoint = std::sqrt(slideV1RelX * slideV1RelX + slideV1RelY * slideV1RelY);
            const float penetration = CLIP_RADIUS_WALLS - distToPoint;

            if (penetration >= MIN_PENETRATION) {
                // Figure out the normal to move back along and move back
                const float normDirX = slideV1RelX / distToPoint;
                const float normDirY = slideV1RelY / distToPoint;

                CollisionResponse response;
                response.moveX = penetration * normDirX;
                response.moveY = penetration * normDirY;
                gCollisionResponses.push_back(response);
            }
        }
    }
    else if (distAlongLine >= lineLen) {
        // Maybe colliding with the end point of the line: see how far away we are from it
        if (distAlongLine < lineLen + CLIP_RADIUS_WALLS) {
            // Colliding with the line start point.
            // Get the distance to it to determine the penetration, then move back along the normal.
            const float slideV2RelX = slideX - lineP2x;
            const float slideV2RelY = slideY - lineP2y;
            const float distToPoint = std::sqrt(slideV2RelX * slideV2RelX + slideV2RelY * slideV2RelY);
            const float penetration = CLIP_RADIUS_WALLS - distToPoint;

            if (penetration >= MIN_PENETRATION) {
                // Figure out the normal to move back along and move back
                const float normDirX = slideV2RelX / distToPoint;
                const float normDirY = slideV2RelY / distToPoint;

                CollisionResponse response;
                response.moveX = penetration * normDirX;
                response.moveY = penetration * normDirY;
                gCollisionResponses.push_back(response);
            }
        }
    }
    else {
        // Just colliding with the line plane itself.
        // Move the slide thing back by the penetration distance.
        const float penetration = CLIP_RADIUS_WALLS - distToLine;

        if (penetration >= MIN_PENETRATION) {
            CollisionResponse response;
            response.moveX = penetration * lineNormX;
            response.moveY = penetration * lineNormY;
            gCollisionResponses.push_back(response);
        }
    }
}

void slideCollideWithSubSectorLines(subsector_t& subSector) noexcept {
    seg_t* pCurSeg = subSector.firstline;
    seg_t* const pEndSeg = pCurSeg + subSector.numsublines;

    while (pCurSeg < pEndSeg) {
        slideCollideWithLineSeg(*pCurSeg);
        ++pCurSeg;
    }
}

void slideCollideWithSubSector(subsector_t& subSector) noexcept {
    slideCollideWithSectorThings(*subSector.sector);
    slideCollideWithSubSectorLines(subSector);
}

//----------------------------------------------------------------------------------------------------------------------
// Slide against a BSP tree node and it's children (lines & things)
//----------------------------------------------------------------------------------------------------------------------
void slideCollideWithBspTree(node_t& rootNode) noexcept {
    // Is this node actual pointing to a sub sector?
    if (isBspNodeASubSector(&rootNode)) {
        // Process the sub sector.
        // N.B: Need to fix up the pointer as well due to the lowest bit set as a flag!
        subsector_t* const pSubSector = (subsector_t*) getActualBspNodePtr(&rootNode);
        slideCollideWithSubSector(*pSubSector);
    }
    else {
        // This is a node: first collide with the front side of the seg
        const uint32_t side = PointOnVectorSide(gSlideX, gSlideY, rootNode.Line);
        slideCollideWithBspTree(*(node_t*) rootNode.Children[side]);

        // Determine if we are close enough to the other side of the split to collide with that
        const float slideX = fixed16ToFloat(gSlideX);
        const float slideY = fixed16ToFloat(gSlideY);

        const float nodeP1x = fixed16ToFloat(rootNode.Line.x);
        const float nodeP1y = fixed16ToFloat(rootNode.Line.y);
        const float nodeVecX = fixed16ToFloat(rootNode.Line.dx);
        const float nodeVecY = fixed16ToFloat(rootNode.Line.dy);
        const float nodeLen = std::sqrt(nodeVecX * nodeVecX + nodeVecY * nodeVecY);

        const float nodeDx = nodeVecX / nodeLen;
        const float nodeDy = nodeVecY / nodeLen;
        const float nodeNx = -nodeDy;
        const float nodeNy = nodeDx;

        const float slideRx = slideX - nodeP1x;
        const float slideRy = slideY - nodeP1y;
        const float distToNode = std::abs(slideRx * nodeNx + slideRy * nodeNy);

        if (distToNode < (float) BSP_RADIUS) {
            slideCollideWithBspTree(*(node_t*) rootNode.Children[side ^ 1]);
        }
    }
}

void init() noexcept {
    gCollisionResponses.reserve(32);
}

void shutdown() noexcept {
    gCollisionResponses.clear();
    gCollisionResponses.shrink_to_fit();
}

void doSliding(mobj_t& mo) noexcept {
    gpSlideThing = &mo;

    bool bDidCollide = false;
    gSlideX = gpSlideThing->x + mo.momx;
    gSlideY = gpSlideThing->y + mo.momy;

    // 8 resolve steps - hopefully that should be plenty for most situations
    for (int32_t resolveIter = 0; resolveIter < 8; resolveIter++) {
        // See what we are colliding with (if anything)
        ++gValidCount;
        slideCollideWithBspTree(*gpBSPTreeRoot);

        if (gCollisionResponses.empty())
            break;
        
        // Figure out the collision response.
        // Resolve only the smallest response in this round on the basis that the smallest thing is probably what we are closest to.
        bDidCollide = true;

        float smallestResponseX = gCollisionResponses[0].moveX;
        float smallestResponseY = gCollisionResponses[0].moveY;
        float smallestResponseMag = smallestResponseX * smallestResponseX + smallestResponseY * smallestResponseY;

        for (uint32_t respIdx = 1; respIdx < gCollisionResponses.size(); ++respIdx) {
            const CollisionResponse& response = gCollisionResponses[respIdx];
            float responseMag = response.moveX * response.moveX + response.moveY * response.moveY;

            if (responseMag < smallestResponseMag) {
                smallestResponseMag = responseMag;
                smallestResponseX = response.moveX;
                smallestResponseY = response.moveY;
            }
        }

        // Apply the collision responses and clear the list for next time
        gCollisionResponses.clear();
        gSlideX += floatToFixed16(smallestResponseX);
        gSlideY += floatToFixed16(smallestResponseY);
    }

    // Update momentum to be based on where we actually went to.
    // Don't allow it to exceed the original momentum strength however!
    if (bDidCollide) {
        const float origMomX = fixed16ToFloat(mo.momx);
        const float origMomY = fixed16ToFloat(mo.momy);

        const Fixed newMomXFrac = gSlideX - gpSlideThing->x;
        const Fixed newMomYFrac = gSlideY - gpSlideThing->y;
        const float newMomX = fixed16ToFloat(newMomXFrac);
        const float newMomY = fixed16ToFloat(newMomYFrac);

        const float origMomAmt = std::sqrt(origMomX * origMomX + origMomY * origMomY);
        const float newMomAmt = std::sqrt(newMomX * newMomX + newMomY * newMomY);

        if (newMomAmt > origMomAmt) {
            // New momentum too much! Renormalize first before reapplying:
            const float renormalize = origMomAmt / newMomAmt;
            const float fixedNewMomX = fixed16ToFloat(mo.momx) * renormalize;
            const float fixedNewMomY = fixed16ToFloat(mo.momy) * renormalize;

            mo.momx = floatToFixed16(fixedNewMomX);
            mo.momy = floatToFixed16(fixedNewMomY);
        } else {
            mo.momx = newMomXFrac;
            mo.momy = newMomYFrac;
        }
    }

    // After we are done check if we crossed any trigger lines
    SL_CheckSpecialLines(
        gpSlideThing->x,
        gpSlideThing->y,
        gSlideX,
        gSlideY
    );
}

END_NAMESPACE(Slide)
