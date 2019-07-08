#include "Slide.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"

#define CLIPRADIUS  23
#define SIDE_ON 0
#define SIDE_FRONT  1
#define SIDE_BACK   -1

Fixed       gSlideX;        // The final position
Fixed       gSlideY;       
line_t*     gSpecialLine;

static Fixed gSlideDx;       // Current move for completablefrac
static Fixed gSlideDy;
static Fixed gEndBox[4];     // Final proposed position
static Fixed gBlockFrac;     // The fraction of move that gets completed
static Fixed gBlockNVx;      // The vector of the line that blocks move
static Fixed gBlockNVy;

// p1, p2 are line endpoints
// p3, p4 are move endpoints
static int32_t  gP1x;
static int32_t  gP1y;
static int32_t  gP2x;
static int32_t  gP2y;
static int32_t  gP3x;
static int32_t  gP3y;
static int32_t  gP4x;
static int32_t  gP4y;

static Fixed    gNVx;            // Normalized line vector
static Fixed    gNVy;
static mobj_t*  gSlideThing;

static line_t **gList;
static line_t *gLd;

static int SL_PointOnSide2(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3)
{
    int nx, ny;
    int dist;

    x1 = (x1-x2);
    y1 = (y1-y2);

    nx = (y3-y2);
    ny = (x2-x3);

    dist = fixedMul(x1, nx);
    dist += fixedMul(y1, ny);

    if (dist < 0)
        return SIDE_BACK;
    return SIDE_FRONT;
}

/*
===================
=
= P_SlideMove
=
===================
*/
void P_SlideMove(mobj_t* mo)
{
    Fixed   dx, dy;
    Fixed   rx, ry;
    int     i;
    Fixed   frac, slide;

    gSlideThing = mo;
    dx = gSlideThing->momx;
    dy = gSlideThing->momy;
    gSlideX = gSlideThing->x;
    gSlideY = gSlideThing->y;

// perform a maximum of three bumps

    for (i=0 ; i<3 ; i++) {
        frac = P_CompletableFrac (dx,dy);

        if (frac != 0x10000)
            frac -= 0x1000;

        if (frac < 0)
            frac = 0;

        rx = fixedMul(frac, dx);
        ry = fixedMul(frac, dy);
        gSlideX += rx;
        gSlideY += ry;

    //
    // made it the entire way
    //
        if (frac == 0x10000) {
            gSlideThing->momx = dx;
            gSlideThing->momy = dy;
            SL_CheckSpecialLines (gSlideThing->x, gSlideThing->y
                , gSlideX, gSlideY);
            return;
        }

    //
    // project the remaining move along the line that blocked movement
    //
        dx -= rx;
        dy -= ry;
        slide = fixedMul(dx, gBlockNVx);
        slide += fixedMul(dy, gBlockNVy);
        dx = fixedMul(slide, gBlockNVx);
        dy = fixedMul(slide, gBlockNVy);
    }

//
// some hideous situation has happened that won't let the player slide
//
    gSlideX = gSlideThing->x;
    gSlideY = gSlideThing->y;
    gSlideThing->momx = gSlideThing->momy = 0;
}


//---------------------------------------------------------------------------------------------------------------------
// Returns the fraction of the move that is completable
//---------------------------------------------------------------------------------------------------------------------
Fixed P_CompletableFrac(Fixed dx, Fixed dy) {
    gBlockFrac = 0x10000;    // The entire dist until shown otherwise
    gSlideDx = dx;
    gSlideDy = dy;
    
    gEndBox[BOXTOP] = gSlideY + CLIPRADIUS * FRACUNIT;
    gEndBox[BOXBOTTOM] = gSlideY - CLIPRADIUS * FRACUNIT;
    gEndBox[BOXRIGHT] = gSlideX + CLIPRADIUS * FRACUNIT;
    gEndBox[BOXLEFT] = gSlideX - CLIPRADIUS * FRACUNIT;

    if (dx > 0) {
        gEndBox[BOXRIGHT] += dx;
    }
    else {
        gEndBox[BOXLEFT] += dx;
    }
    
    if (dy > 0) {
        gEndBox[BOXTOP] += dy;
    }
    else {
        gEndBox[BOXBOTTOM] += dy;
    }

    ++gValidCount;
    
    // Check lines
    int xl = (gEndBox[BOXLEFT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int xh = (gEndBox[BOXRIGHT] - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int yl = (gEndBox[BOXBOTTOM] - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    int yh = (gEndBox[BOXTOP] - gBlockMapOriginY) >> MAPBLOCKSHIFT;

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
            BlockLinesIterator(bx, by, SL_CheckLine);
        }
    }
    
    // Examine results
    if (gBlockFrac < 0x1000) {
        gBlockFrac = 0;
        gSpecialLine = 0;
        return 0;           // Can't cross anything on a bad move, solid wall or thing
    }

    return gBlockFrac;
}

int32_t SL_PointOnSide(int32_t x, int32_t y)
{
    int     dx, dy, dist;

    dx = x - gP1x;
    dy = y - gP1y;

    dist = fixedMul(dx, gNVx);
    dist += fixedMul(dy, gNVy);

    if (dist > FRACUNIT)
        return SIDE_FRONT;
    if (dist < -FRACUNIT)
        return SIDE_BACK;

    return SIDE_ON;
}

Fixed SL_CrossFrac()
{
    int     dx, dy, dist1, dist2, frac;

// project move start and end points onto line normal
    dx = gP3x - gP1x;
    dy = gP3y - gP1y;

    dist1 = fixedMul(dx, gNVx);
    dist1 += fixedMul(dy, gNVy);

    dx = gP4x - gP1x;
    dy = gP4y - gP1y;

    dist2 = fixedMul(dx, gNVx);
    dist2 += fixedMul(dy, gNVy);

    if ( (dist1 < 0) == (dist2 < 0) )
        return FRACUNIT;        // doesn't cross

    frac = fixedDiv(dist1, dist1 - dist2);
    return frac;
}

bool CheckLineEnds()
{
    int     snx, sny;       // sight normals
    int     dist1, dist2;
    int     dx, dy;

    snx = gP4y-gP3y;
    sny = -(gP4x-gP3x);

    dx = gP1x - gP3x;
    dy = gP1y - gP3y;

    dist1 = fixedMul(dx, snx);
    dist1 += fixedMul(dy, sny);

    dx = gP2x - gP3x;
    dy = gP2y - gP3y;

    dist2 = fixedMul(dx, snx);
    dist2 += fixedMul(dy, sny);

    if ( (dist1<0) == (dist2<0) )
        return true;

    return false;
}

/*
====================
=
= ClipToLine
=
= Call with p1 and p2 set to the endpoints
= and nvx, nvy set to normalized vector
= Assumes the start point is definately on the front side of the line
= returns the fraction of the current move that crosses the line segment
====================
*/
void ClipToLine()
{
    Fixed frac;
    int         side2, side3;

//
// adjust start so it will be the first point contacted on the player
// circle
//

// p3, p4 are move endpoints

    gP3x = gSlideX - CLIPRADIUS*gNVx;
    gP3y = gSlideY - CLIPRADIUS*gNVy;

    gP4x = gP3x + gSlideDx;
    gP4y = gP3y + gSlideDy;

//
// if the adjusted point is on the other side of the line, the endpoint
// must be checked
//
    side2 = SL_PointOnSide (gP3x, gP3y);

    if (side2 == SIDE_BACK)
    {

        return; // !!! ClipToPoint and slide along normal to line
    }

    side3 = SL_PointOnSide(gP4x, gP4y);
    if (side3 == SIDE_ON)
        return;     // the move goes flush with the wall
    if (side3 == SIDE_FRONT)
        return;     // moves doesn't cross line

    if (side2 == SIDE_ON)
    {
        frac = 0;       // moves towards the line
        goto blockmove;
    }

//
// the line endpoints must be on opposite sides of the move trace
//

//
// find the fractional intercept
//
    frac = SL_CrossFrac ();

    if (frac < gBlockFrac)
    {
blockmove:
        gBlockFrac = frac;
        gBlockNVx = -gNVy;
        gBlockNVy = gNVx;
    }
}

/*
==================
=
= SL_CheckLine
=
==================
*/
uint32_t SL_CheckLine(line_t* ld)
{
    Fixed opentop, openbottom;
    sector_t    *front, *back;
    int         side1, temp;

// check bbox first
    if (gEndBox[BOXRIGHT] < ld->bbox[BOXLEFT]
    ||  gEndBox[BOXLEFT] > ld->bbox[BOXRIGHT]
    ||  gEndBox[BOXTOP] < ld->bbox[BOXBOTTOM]
    ||  gEndBox[BOXBOTTOM] > ld->bbox[BOXTOP] )
        return true;

// see if it can possibly block movement

    if (!ld->backsector || ld->flags & ML_BLOCKING)
        goto findfrac;      // explicitly blocking

    front = ld->frontsector;
    back = ld->backsector;

    if (front->floorheight > back->floorheight)
        openbottom = front->floorheight;
    else
        openbottom = back->floorheight;

    if (openbottom - gSlideThing->z > 24*FRACUNIT)
        goto findfrac;      // too big of a step up

    if (front->ceilingheight < back->ceilingheight)
        opentop = front->ceilingheight;
    else
        opentop = back->ceilingheight;

    if (opentop - openbottom >= 56*FRACUNIT)
        return true;        // the line doesn't block movement

// the line definately blocks movement

findfrac:

// p1, p2 are line endpoints
    gP1x = ld->v1.x;
    gP1y = ld->v1.y;
    gP2x = ld->v2.x;
    gP2y = ld->v2.y;

    gNVx = gFineSine[ld->fineangle];
    gNVy = -gFineCosine[ld->fineangle];

    side1 = SL_PointOnSide(gSlideX, gSlideY);
    if (side1 == SIDE_ON)
        return true;
    if (side1 == SIDE_BACK)
    {
        if (!ld->backsector)
            return true;            // don't clip to backs of one sided lines
        temp = gP1x;
        gP1x = gP2x;
        gP2x = temp;
        temp = gP1y;
        gP1y = gP2y;
        gP2y = temp;
        gNVx = -gNVx;
        gNVy = -gNVy;
    }
    ClipToLine ();

    return true;
}

void SL_CheckSpecialLines(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    int xl, xh, yl, yh;
    
    if (x1<x2) {
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

    int bxl = (xl - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int bxh = (xh - gBlockMapOriginX) >> MAPBLOCKSHIFT;
    int byl = (yl - gBlockMapOriginY) >> MAPBLOCKSHIFT;
    int byh = (yh - gBlockMapOriginY) >> MAPBLOCKSHIFT;

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
    
    gSpecialLine = 0;
    ++gValidCount;
    
    int x3, y3, x4, y4;
    int side1, side2;
    
    for (int bx = bxl; bx <= bxh; bx++) {
        for (int by = byl; by <= byh; by++) {
            for (gList = gpBlockMapLineLists[(by * gBlockMapWidth) + bx]; gList[0]; ++gList) {
                gLd = gList[0];
                
                if (!gLd->special)
                    continue;
                
                if (gLd->validcount == gValidCount)
                    continue;   // Line has already been checked
                
                gLd->validcount = gValidCount;
                
                if (xh < gLd->bbox[BOXLEFT]
                ||  xl > gLd->bbox[BOXRIGHT]
                ||  yh < gLd->bbox[BOXBOTTOM]
                ||  yl > gLd->bbox[BOXTOP])
                    continue;
                
                x3 = gLd->v1.x;
                y3 = gLd->v1.y;
                x4 = gLd->v2.x;
                y4 = gLd->v2.y;
                
                side1 = SL_PointOnSide2(x1,y1, x3,y3, x4,y4);
                side2 = SL_PointOnSide2(x2,y2, x3,y3, x4,y4);

                if (side1 == side2)
                    continue;   // Move doesn't cross line
                
                side1 = SL_PointOnSide2(x3,y3, x1,y1, x2,y2);
                side2 = SL_PointOnSide2(x4,y4, x1,y1, x2,y2);

                if (side1 == side2)
                    continue;   // Line doesn't cross move

                gSpecialLine = gLd;
                return;
            }
        }
    }
}
