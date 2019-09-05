#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Things/MapObj.h"

//----------------------------------------------------------------------------------------------------------------------
// Module that handles traversing the BSP tree, so we can produce lists of things to draw.
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Renderer)

static uint32_t gCheckCoord[9][4] = {
    { BOXRIGHT, BOXTOP, BOXLEFT, BOXBOTTOM },       // Above,Left
    { BOXRIGHT, BOXTOP, BOXLEFT, BOXTOP },          // Above,Center
    { BOXRIGHT, BOXBOTTOM, BOXLEFT, BOXTOP },       // Above,Right
    { BOXLEFT, BOXTOP, BOXLEFT, BOXBOTTOM },        // Center,Left
    { UINT32_MAX, 0, 0, 0 },                        // Center,Center
    { BOXRIGHT, BOXBOTTOM, BOXRIGHT, BOXTOP },      // Center,Right
    { BOXLEFT, BOXTOP, BOXRIGHT, BOXBOTTOM },       // Below,Left
    { BOXLEFT, BOXBOTTOM, BOXRIGHT, BOXBOTTOM },    // Below,Center
    { BOXLEFT, BOXBOTTOM, BOXRIGHT, BOXTOP }        // Below,Right
};

//----------------------------------------------------------------------------------------------------------------------
// Given a sector pointer, and if I hadn't already rendered the sprites, make valid sprites for the sprite list.
//----------------------------------------------------------------------------------------------------------------------
static void addSectorSpritesToFrame(sector_t& sector) noexcept {
    if (sector.validcount != gValidCount) {     // Has this been processed?
        sector.validcount = gValidCount;        // Mark it
        mobj_t* pThing = sector.thinglist;      // Init the thing list

        // Traverse the linked list and add each sprite
        while (pThing) {
            addSpriteToFrame(*pThing);
            pThing = pThing->snext;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Given a subsector pointer, pass all walls to the rendering engine. Also pass all the sprites.
//----------------------------------------------------------------------------------------------------------------------
static void addSubsectorToFrame(subsector_t& sub) noexcept {
    sector_t& sector = *sub.sector;     // Get the front sector
    addSectorSpritesToFrame(sector);    // Prepare sprites for rendering

    // Pass all line segments in the subsector to the renderer
    seg_t* pLineSeg = sub.firstline;
    seg_t* const pEndLineSeg = pLineSeg + sub.numsublines;

    while (pLineSeg < pEndLineSeg) {
        addSegToFrame(*pLineSeg);
        ++pLineSeg;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Check if any part of the BSP bounding box is touching the view arc.
// Also project the width of the box to screen coords to see if it is too small to even bother with.
// If I should process this box then return 'true'.
//----------------------------------------------------------------------------------------------------------------------
static bool checkBBox(const Fixed bspcoord[BOXCOUNT]) noexcept {
    // FIXME: This check is BROKEN for high resolutions
    #if 1
        return true;
    #else

    // Left and right angles for view
    angle_t angle1;
    angle_t angle2;

    // Find the corners of the box that define the edges from current viewpoint.
    // First use the box pointer:
    {
        uint32_t* pBox = &gCheckCoord[0][0];            // Init to the base of the table (Above)
        if (gViewYFrac < bspcoord[BOXTOP]) {            // Off the top?
            pBox += 12;                                 // Index to center
            if (gViewYFrac <= bspcoord[BOXBOTTOM]) {    // Off the bottom?
                pBox += 12;                             // Index to below
            }
        }

        if (gViewXFrac > bspcoord[BOXLEFT]) {           // Check if off the left edge
            pBox += 4;                                  // Center x
            if (gViewXFrac >= bspcoord[BOXRIGHT]) {     // Is it off the right?
                pBox += 4;
            }
        }

        if (pBox[0] == -1) {    // Center node?
            return true;        // I am in the center of the box, process it!!
        }

        // I now have in 3 Space the endpoints of the BSP box, now project it to the screen
        // and see if it is either off the screen or too small to even care about
        angle1 = PointToAngle(gViewXFrac, gViewYFrac, bspcoord[pBox[0]], bspcoord[pBox[1]]) - gViewAngleBAM;    // What is the projected angle?
        angle2 = PointToAngle(gViewXFrac, gViewYFrac, bspcoord[pBox[2]], bspcoord[pBox[3]]) - gViewAngleBAM;    // Now the rightmost angle
    }

    // Use span and tspan
    {
        const angle_t span = angle1 - angle2;   // What is the span of the angle?
        if (span >= ANG180) {                   // Whoa... I must be sitting on the line or it's in my face!
            return true;                        // Process this one...
        }

        // angle1 must be treated as signed, so to see if it is either >-clipangle and < clipangle
        // I add clipangle to the angle to adjust the 0 center and compare to clipangle * 2

        angle_t tspan = angle1 + gClipAngleBAM;
        if (tspan > gDoubleClipAngleBAM) {         // Possibly off the left edge
            tspan -= gDoubleClipAngleBAM;
            if (tspan >= span) {                    // Off the left side?
                return false;                       // Don't bother, it's off the left side
            }
            angle1 = gClipAngleBAM;                 // Clip the left edge
        }

        tspan = gClipAngleBAM - angle2;            // Move from a zero base of "clipangle"
        if (tspan > gDoubleClipAngleBAM) {         // Possible off the right edge
            tspan -= gDoubleClipAngleBAM;
            if (tspan >= span) {                    // The entire span is off the right edge?
                return false;                       // Too far right!
            }
            angle2 = -(int32_t) gClipAngleBAM;      // Clip the right edge angle
        }

        // See if any part of the contained area could be visible
        angle1 = (angle1 + ANG90) >> (ANGLETOFINESHIFT + 1);        // Rotate 90 degrees and make table index
        angle2 = (angle2 + ANG90) >> (ANGLETOFINESHIFT + 1);
    }

    angle1 = gViewAngleToX[angle1];      // Get the screen coords
    angle2 = gViewAngleToX[angle2];

    if (angle1 == angle2) {     // Is the run too small?
        return false;           // Don't bother rendering it then
    }

    --angle2;

    // FIXME: Do we need to reimplement?
    #if 0
        // Use start
        {
            cliprange_t* pSolid = gSolidsegs;                   // Pointer to the solid walls
            if (pSolid->rightX < (int32_t) angle2) {            // Scan through the sorted list
                do {
                    ++pSolid;                                   // Next entry
                } while (pSolid->rightX < (int32_t) angle2);
            }

            if ((int32_t) angle1 >= pSolid->leftX && (int32_t) angle2 <= pSolid->rightX) {
                return false;   // This block is behind a solid wall!
            }
        }
    #endif

    return true;    // Process me!

    #endif
}

//---------------------------------------------------------------------------------------------------------------------
// Traverse the BSP tree starting from a tree node (Or sector) and recursively subdivide if needed.
// Use a cross product from the line cast from the viewxy to the bspxy and the bsp line itself.
//---------------------------------------------------------------------------------------------------------------------
static void addBspNodeToFrame(const node_t* const pNode) noexcept {
    // Is this node actual pointing to a sub sector?
    if (isBspNodeASubSector(pNode)) {
        // Process the sub sector.
        // N.B: Need to fix up the pointer as well due to the lowest bit set as a flag!
        subsector_t* const pSubSector = (subsector_t*) getActualBspNodePtr(pNode);
        addSubsectorToFrame(*pSubSector);
        return;
    }

    // If we have filled the screen then exit now - don't traverse the BSP any further
    if (gNumFullSegCols >= g3dViewWidth)
        return;

    // Decide which side the view point is on
    uint32_t side = PointOnVectorSide(gViewXFrac, gViewYFrac, pNode->Line);     // Is this the front side?
    addBspNodeToFrame((const node_t*) pNode->Children[side]);                   // Process the side closer to me
    side ^= 1;                                                                  // Swap the side

    if (checkBBox(pNode->bbox[side])) {                                 // Is the viewing rect on both sides?
        addBspNodeToFrame((const node_t*) pNode->Children[side]);       // Render the back side
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Find all walls that can be rendered in the current view plane. I make it handle the whole
// screen by placing fake posts on the farthest left and right sides in solidsegs 0 and 1.
//---------------------------------------------------------------------------------------------------------------------
void doBspTraversal() noexcept {
    ++gValidCount;                          // For sprite recursion
    addBspNodeToFrame(gpBSPTreeRoot);       // Begin traversing the BSP tree for all walls in render range
}

END_NAMESPACE(Renderer)
