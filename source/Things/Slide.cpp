#include "Slide.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"
#include <algorithm>

static constexpr int32_t    CLIPRADIUS  = 23;
static constexpr uint32_t   SIDE_ON     = 0;
static constexpr uint32_t   SIDE_FRONT  = 1;
static constexpr uint32_t   SIDE_BACK   = UINT32_MAX;

Fixed       gSlideX;        // The final position
Fixed       gSlideY;
line_t*     gpSpecialLine;

static Fixed    gSlideDx;       // Current move for completablefrac
static Fixed    gSlideDy;
static Fixed    gEndBox[4];     // Final proposed position
static Fixed    gBlockFrac;     // The fraction of move that gets completed
static Fixed    gBlockNVx;      // The vector of the line that blocks move
static Fixed    gBlockNVy;

// p1, p2 are line endpoints
// p3, p4 are move endpoints
static Fixed        gP1x;
static Fixed        gP1y;
static Fixed        gP2x;
static Fixed        gP2y;
static Fixed        gP3x;
static Fixed        gP3y;
static Fixed        gP4x;
static Fixed        gP4y;

static Fixed        gNVx;   // Normalized line vector
static Fixed        gNVy;
static mobj_t*      gpSlideThing;

static line_t**     gppList;
static line_t*      gpLd;

static uint32_t SL_PointOnSide2(
    int32_t x1,
    int32_t y1,
    const int32_t x2,
    const int32_t y2,
    const int32_t x3,
    const int32_t y3
) noexcept {
    x1 = x1 - x2;
    y1 = y1 - y2;
    int32_t nx = y3 - y2;
    int32_t ny = x2 - x3;

    Fixed dist = fixed16Mul(x1, nx);
    dist += fixed16Mul(y1, ny);

    if (dist < 0) {
        return SIDE_BACK;
    } else {
        return SIDE_FRONT;
    }
}

void P_SlideMove(mobj_t& mo) noexcept {
    gpSlideThing = &mo;
    Fixed dx = mo.momx;
    Fixed dy = mo.momy;
    gSlideX = gpSlideThing->x;
    gSlideY = gpSlideThing->y;

    // Perform a maximum of three bumps
    for (int32_t i = 0 ; i < 3 ; i++) {
        Fixed frac = P_CompletableFrac(dx, dy);

        if (frac != 0x10000) {
            frac -= 0x1000;
        }

        if (frac < 0) {
            frac = 0;
        }

        const Fixed rx = fixed16Mul(frac, dx);
        const Fixed ry = fixed16Mul(frac, dy);
        gSlideX += rx;
        gSlideY += ry;

        // Made it the entire way
        if (frac == 0x10000) {
            gpSlideThing->momx = dx;
            gpSlideThing->momy = dy;
            SL_CheckSpecialLines(
                gpSlideThing->x,
                gpSlideThing->y,
                gSlideX,
                gSlideY
            );
            return;
        }

        // Project the remaining move along the line that blocked movement
        dx -= rx;
        dy -= ry;
        Fixed slide = fixed16Mul(dx, gBlockNVx);
        slide += fixed16Mul(dy, gBlockNVy);
        dx = fixed16Mul(slide, gBlockNVx);
        dy = fixed16Mul(slide, gBlockNVy);
    }

    // Some hideous situation has happened that won't let the player slide
    gSlideX = gpSlideThing->x;
    gSlideY = gpSlideThing->y;
    gpSlideThing->momx = gpSlideThing->momy = 0;
}


//----------------------------------------------------------------------------------------------------------------------
// Returns the fraction of the move that is completable
//----------------------------------------------------------------------------------------------------------------------
Fixed P_CompletableFrac(Fixed dx, Fixed dy) noexcept {
    gBlockFrac = 0x10000;   // The entire dist until shown otherwise
    gSlideDx = dx;
    gSlideDy = dy;

    gEndBox[BOXTOP] = gSlideY + CLIPRADIUS * FRACUNIT;
    gEndBox[BOXBOTTOM] = gSlideY - CLIPRADIUS * FRACUNIT;
    gEndBox[BOXRIGHT] = gSlideX + CLIPRADIUS * FRACUNIT;
    gEndBox[BOXLEFT] = gSlideX - CLIPRADIUS * FRACUNIT;

    if (dx > 0) {
        gEndBox[BOXRIGHT] += dx;
    } else {
        gEndBox[BOXLEFT] += dx;
    }

    if (dy > 0) {
        gEndBox[BOXTOP] += dy;
    } else {
        gEndBox[BOXBOTTOM] += dy;
    }

    ++gValidCount;

    // Check lines
    int32_t xl = (gEndBox[BOXLEFT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int32_t xh = (gEndBox[BOXRIGHT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int32_t yl = (gEndBox[BOXBOTTOM] - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    int32_t yh = (gEndBox[BOXTOP] - gBlockMapOriginY) >> MAPBLOCKSHIFT;

    xl = std::max(xl, 0);
    yl = std::max(yl, 0);

    if (xh >= 0 && yh >= 0) {
        if (xh >= (int32_t) gBlockMapWidth) {
            xh = (int32_t) gBlockMapWidth - 1;
        }

        if (yh >= (int32_t) gBlockMapHeight) {
            yh = (int32_t) gBlockMapHeight - 1;
        }

        for (int32_t bx = xl; bx <= xh; bx++) {
            for (int32_t by = yl; by <= yh; by++) {
                BlockLinesIterator((uint32_t) bx, (uint32_t) by, SL_CheckLine);
            }
        }
    }

    // Examine results
    if (gBlockFrac < 0x1000) {
        gBlockFrac = 0;
        gpSpecialLine = nullptr;
        return 0;   // Can't cross anything on a bad move, solid wall or thing
    }

    return gBlockFrac;
}

uint32_t SL_PointOnSide(const Fixed x, const Fixed y) noexcept {
    Fixed dx = x - gP1x;
    Fixed dy = y - gP1y;

    Fixed dist = fixed16Mul(dx, gNVx);
    dist += fixed16Mul(dy, gNVy);

    if (dist > FRACUNIT) {
        return SIDE_FRONT;
    }

    if (dist < -FRACUNIT) {
        return SIDE_BACK;
    }

    return SIDE_ON;
}

Fixed SL_CrossFrac() noexcept {
    // Project move start and end points onto line normal
    Fixed dx = gP3x - gP1x;
    Fixed dy = gP3y - gP1y;

    Fixed dist1 = fixed16Mul(dx, gNVx);
    dist1 += fixed16Mul(dy, gNVy);
    dx = gP4x - gP1x;
    dy = gP4y - gP1y;

    Fixed dist2 = fixed16Mul(dx, gNVx);
    dist2 += fixed16Mul(dy, gNVy);

    if ((dist1 < 0) == (dist2 < 0)) {
        return FRACUNIT;    // Doesn't cross
    }

    Fixed frac = fixed16Div(dist1, dist1 - dist2);
    return frac;
}

bool CheckLineEnds() noexcept {
    Fixed snx = gP4y - gP3y;
    Fixed sny = -(gP4x - gP3x);
    Fixed dx = gP1x - gP3x;
    Fixed dy = gP1y - gP3y;

    Fixed dist1 = fixed16Mul(dx, snx);
    dist1 += fixed16Mul(dy, sny);

    dx = gP2x - gP3x;
    dy = gP2y - gP3y;

    Fixed dist2 = fixed16Mul(dx, snx);
    dist2 += fixed16Mul(dy, sny);

    if ((dist1 < 0) == (dist2 < 0)) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// Call with p1 and p2 set to the endpoints and nvx, nvy set to normalized vector.
// Assumes the start point is definately on the front side of the line returns the fraction
// of the current move that crosses the line segment.
//----------------------------------------------------------------------------------------------------------------------
void ClipToLine() noexcept {
    Fixed frac = 0;
    
    // Adjust start so it will be the first point contacted on the player circle:
    // p3, p4 are move endpoints.
    gP3x = gSlideX - CLIPRADIUS * gNVx;
    gP3y = gSlideY - CLIPRADIUS * gNVy;
    gP4x = gP3x + gSlideDx;
    gP4y = gP3y + gSlideDy;

    // If the adjusted point is on the other side of the line, the endpoint must be checked
    const uint32_t side2 = SL_PointOnSide(gP3x, gP3y);

    if (side2 == SIDE_BACK) {
        return;     // !!! ClipToPoint and slide along normal to line
    }

    const uint32_t side3 = SL_PointOnSide(gP4x, gP4y);

    if (side3 == SIDE_ON) {
        return;     // The move goes flush with the wall
    }

    if (side3 == SIDE_FRONT) {
        return;     // Moves doesn't cross line
    }

    if (side2 == SIDE_ON) {
        frac = 0;   // Moves towards the line
        goto blockmove;
    }

    // The line endpoints must be on opposite sides of the move trace.
    // Find the fractional intercept:
    frac = SL_CrossFrac();

    if (frac < gBlockFrac) {
    blockmove:
        gBlockFrac = frac;
        gBlockNVx = -gNVy;
        gBlockNVy = gNVx;
    }
}

bool SL_CheckLine(line_t& ld) noexcept {
    ASSERT(ld.frontsector);

    // Check bbox first
    if ((gEndBox[BOXRIGHT] < ld.bbox[BOXLEFT]) ||
        (gEndBox[BOXLEFT] > ld.bbox[BOXRIGHT]) ||
        (gEndBox[BOXTOP] < ld.bbox[BOXBOTTOM]) ||
        (gEndBox[BOXBOTTOM] > ld.bbox[BOXTOP])
    ) {
        return true;
    }

    // See if it can possibly block movement
    do {
        if ((!ld.backsector) || ((ld.flags & ML_BLOCKING) != 0)) {
            break;  // Explicitly blocking
        }

        sector_t& front = *ld.frontsector;
        sector_t& back = *ld.backsector;

        Fixed openbottom;

        if (front.floorheight > back.floorheight) {
            openbottom = front.floorheight;
        } else {
            openbottom = back.floorheight;
        }

        if (openbottom - gpSlideThing->z > 24 * FRACUNIT) {
            break;  // Too big of a step up
        }

        Fixed opentop;

        if (front.ceilingheight < back.ceilingheight) {
            opentop = front.ceilingheight;
        } else {
            opentop = back.ceilingheight;
        }

        if (opentop - openbottom >= 56 * FRACUNIT) {
            return true;    // The line doesn't block movement
        }
    } while (false);

    // The line definately blocks movement: p1, p2 are line endpoints
    gP1x = ld.v1.x;
    gP1y = ld.v1.y;
    gP2x = ld.v2.x;
    gP2y = ld.v2.y;

    gNVx = gFineSine[ld.fineangle];
    gNVy = -gFineCosine[ld.fineangle];
    const uint32_t side1 = SL_PointOnSide(gSlideX, gSlideY);

    if (side1 == SIDE_ON) {
        return true;
    }

    if (side1 == SIDE_BACK) {
        if (!ld.backsector) {
            return true;    // Don't clip to backs of one sided lines
        }

        Fixed temp = gP1x;
        gP1x = gP2x;
        gP2x = temp;
        temp = gP1y;
        gP1y = gP2y;
        gP2y = temp;
        gNVx = -gNVx;
        gNVy = -gNVy;
    }

    ClipToLine();
    return true;
}

void SL_CheckSpecialLines(
    const int32_t x1,
    const int32_t y1,
    const int32_t x2,
    const int32_t y2
) noexcept {
    int32_t xl;
    int32_t xh;
    int32_t yl;
    int32_t yh;

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

    if (bxl < 0) {
        bxl = 0;
    }

    if (byl < 0) {
        byl = 0;
    }

    if (bxh >= (int32_t) gBlockMapWidth) {
        bxh = (int32_t) gBlockMapWidth - 1;
    }

    if (byh >= (int32_t) gBlockMapHeight) {
        byh = (int32_t) gBlockMapHeight - 1;
    }

    gpSpecialLine = nullptr;
    ++gValidCount;

    for (uint32_t bx = (uint32_t) bxl; bx <= (uint32_t) bxh; bx++) {
        for (uint32_t by = (uint32_t) byl; by <= (uint32_t) byh; by++) {
            for (gppList = gpBlockMapLineLists[(by * gBlockMapWidth) + bx]; gppList[0]; ++gppList) {
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
