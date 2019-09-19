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
// Transform a 2d xy point to view space
//----------------------------------------------------------------------------------------------------------------------
static inline void transformXYPointToViewSpace(vertexf_t& point) noexcept {
    // Transform by view position and then view angle; similar to the code in other render functions:
    point.x -= gViewX;
    point.y -= gViewY;

    const float viewCos = gViewCos;
    const float viewSin = gViewSin;
    const float rotatedX = viewCos * point.x - viewSin * point.y;
    const float rotatedY = viewSin * point.x + viewCos * point.y;

    point.x = rotatedX;
    point.y = rotatedY;
}

//----------------------------------------------------------------------------------------------------------------------
// Transform a 2d xy point to clip space.
// The 'y' component becomes the 'w' component in clip space.
//----------------------------------------------------------------------------------------------------------------------
static inline void transformXYPointToClipSpace(vertexf_t& point) noexcept {
    // Notes:
    //  (1) We treat 'y' as if it were 'z' for the purposes of these calculations, since the
    //      projection matrix has 'z' as the depth value and not y (Doom coord sys).
    //  (2) We assume that the point always starts off with an implicit 'w' value of '1'.
    //  (3) W = Y in this calculation, so I don't bother assigning to it.
    point.x *= gProjMatrix.r0c0;
}

//----------------------------------------------------------------------------------------------------------------------
// Some basic rejection checks to see if we should process a BSP node.
//----------------------------------------------------------------------------------------------------------------------
static bool checkBBox(const Fixed bspcoord[BOXCOUNT]) noexcept {
    // Get the float coords first
    const float boxLx = fixed16ToFloat(bspcoord[BOXLEFT]);
    const float boxRx = fixed16ToFloat(bspcoord[BOXRIGHT]);
    const float boxTy = fixed16ToFloat(bspcoord[BOXTOP]);
    const float boxBy = fixed16ToFloat(bspcoord[BOXBOTTOM]);

    // Makeup the 4 box points and transform to view space
    vertexf_t p1 = { boxLx, boxTy };
    vertexf_t p2 = { boxRx, boxTy };
    vertexf_t p3 = { boxRx, boxBy };
    vertexf_t p4 = { boxLx, boxBy };
    transformXYPointToViewSpace(p1);
    transformXYPointToViewSpace(p2);
    transformXYPointToViewSpace(p3);
    transformXYPointToViewSpace(p4);

    // If all are behind the camera then we can ignore completely
    const bool bAllPtsBehind = (
        (p1.y < Z_NEAR) &&
        (p2.y < Z_NEAR) &&
        (p3.y < Z_NEAR) &&
        (p4.y < Z_NEAR)
    );

    if (bAllPtsBehind)
        return false;
    
    // Transform to clip space and see if the box is offscreen to the left or right
    transformXYPointToClipSpace(p1);
    transformXYPointToClipSpace(p2);
    transformXYPointToClipSpace(p3);
    transformXYPointToClipSpace(p4);

    // See if all the points are either to the left or right of the view frustrum left and right planes.
    // 'y' is now actually the 'w' coordinate value in clip space, so we can do normal clipspace checks here:
    const bool bAllPtsToLeft = (
        (p1.x < -p1.y) &&
        (p2.x < -p2.y) &&
        (p3.x < -p3.y) &&
        (p4.x < -p4.y)
    );

    if (bAllPtsToLeft)
        return false;

    const bool bAllPtsToRight = (
        (p1.x > p1.y) &&
        (p2.x > p2.y) &&
        (p3.x > p3.y) &&
        (p4.x > p4.y)
    );

    if (bAllPtsToRight)
        return false;

    // If we get to here then the BSP node is in bounds, process it!
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Traverse the BSP tree starting from a tree node (Or sector) and recursively subdivide if needed.
// Use a cross product from the line cast from the viewxy to the bspxy and the bsp line itself.
//----------------------------------------------------------------------------------------------------------------------
static void addBspNodeToFrame(node_t* const pNode) noexcept {
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
    addBspNodeToFrame((node_t*) pNode->Children[side]);                         // Process the side closer to me
    side ^= 1;                                                                  // Swap the side

    if (checkBBox(pNode->bbox[side])) {                         // Is the viewing rect on both sides?
        addBspNodeToFrame((node_t*) pNode->Children[side]);     // Render the back side
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Find all walls that can be rendered in the current view plane. I make it handle the whole
// screen by placing fake posts on the farthest left and right sides in solidsegs 0 and 1.
//----------------------------------------------------------------------------------------------------------------------
void doBspTraversal() noexcept {
    ++gValidCount;                          // For sprite recursion
    addBspNodeToFrame(gpBSPTreeRoot);       // Begin traversing the BSP tree for all walls in render range
}

END_NAMESPACE(Renderer)
