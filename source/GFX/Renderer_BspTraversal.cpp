#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "Sprites.h"
#include "Things/Info.h"
#include "Things/MapObj.h"

BEGIN_NAMESPACE(Renderer)

//----------------------------------------------------------------------------------------------------------------------
// By traversing the BSP tree, I will create a viswall_t array describing all walls are are visible to the computer 
// screen, they may be projected off the left and right sides but this is to allow for scaling of the textures
// properly for clipping.
// 
// All backface walls are removed by casting two angles to the end points and seeing if the differance in the angles
// creates a negative number (Reversed). The viswall_t record will contain the leftmost angle in unmodified 3 Space,
// the clipped screen left x and right x and the line segment that needs to be rendered.
//
// I also create all the sprite records (Unsorted) so that they can be merged with the rendering system to 
// handle clipping.
//----------------------------------------------------------------------------------------------------------------------
struct cliprange_t {
    int32_t leftX;      // Left side of post
    int32_t rightX;     // Right side of post
};

static constexpr uint32_t MAXSEGS = 32;     // Maximum number of segs to scan

static uint32_t gSortBuffer[MAXVISSPRITES * 2];

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

static cliprange_t  gSolidsegs[MAXSEGS];    // List of valid ranges to scan through
static cliprange_t* gNewEnd;                // Pointer to the first free entry

//----------------------------------------------------------------------------------------------------------------------
// Get the sprite angle (0-7) to render a thing with
//----------------------------------------------------------------------------------------------------------------------
static uint8_t getThingSpriteAngleForViewpoint(
    const Fixed viewpointX,
    const Fixed viewpointY,
    const mobj_t& thing
) noexcept {
    angle_t ang = PointToAngle(gViewX, gViewY, thing.x, thing.y);           // Get angle to thing
    ang -= thing.angle;                                                     // Adjust for which way the thing is facing
    const uint8_t angleIdx = (ang + (angle_t)((ANG45 / 2) * 9U)) >> 29;     // Compute and return angle index (0-7)
    return angleIdx;
}

//----------------------------------------------------------------------------------------------------------------------
// I have a possible sprite record, transform to 2D coords and then see if it is clipped.
// If the sprite is not clipped then it is prepared for rendering.
//----------------------------------------------------------------------------------------------------------------------
static void addMapObjToFrame(const mobj_t& thing) noexcept {
    // This is a HACK, so I don't draw the player for the 3DO version
    if (thing.player) {
        return;
    }

    // Don't draw the sprite if we have hit the maximum limit
    vissprite_t* pVisSprite = gpEndVisSprite;
    if (pVisSprite >= &gVisSprites[MAXVISSPRITES]) {
        return;
    }

    // Transform the origin point
    Fixed tx = thing.x - gViewX;            // Get the point in 3 Space
    Fixed ty = thing.y - gViewY;
    Fixed tz = fixedMul(tx, gViewCos);      // Rotate around the camera
    tz += fixedMul(ty, gViewSin);           // Add together

    // Ignore sprite if too close to the camera (too large)
    if (tz < MINZ) {
        return;
    }
    
    // Calc the 3Space x coord
    tx = fixedMul(tx, gViewSin);
    tx -= fixedMul(ty, gViewCos);
    
    // Ignore sprite if greater than 45 degrees off the side
    if (tx > (tz << 2) || tx < -(tz << 2)) {
        return;
    }

    // Figure out what sprite, frame and frame angle we want
    const state_t* const pStatePtr = thing.state;
    const uint32_t spriteResourceNum = pStatePtr->SpriteFrame >> FF_SPRITESHIFT;
    const uint32_t spriteFrameNum = pStatePtr->SpriteFrame & FF_FRAMEMASK;
    const uint8_t spriteAngle = getThingSpriteAngleForViewpoint(gViewX, gViewY, thing);

    // Load the current sprite for the thing and the info for the actual sprite to use
    const Sprite* const pSprite = loadSprite(spriteResourceNum);
    ASSERT(spriteFrameNum < pSprite->numFrames);

    const SpriteFrame* const pSpriteFrame = &pSprite->pFrames[spriteFrameNum];
    const SpriteFrameAngle* const pSpriteFrameAngle = &pSpriteFrame->angles[spriteAngle];

    // Store information in a vissprite.
    // I also will clip to screen coords.
    const Fixed xScale = fixedDiv(gCenterX << FRACBITS, tz);        // Get the scale factor
    pVisSprite->xscale = xScale;                                    // Save it
    tx -= (Fixed) pSpriteFrameAngle->leftOffset << FRACBITS;        // Adjust the x to the sprite's x
    int x1 = (fixedMul(tx, xScale) >> FRACBITS) + gCenterX;         // Scale to screen coords

    if (x1 > (int) gScreenWidth) {
        return;     // Off the right side, don't draw!
    }

    int x2 = fixedMul(pSpriteFrameAngle->width, xScale) + x1;

    if (x2 <= 0) {
        return;     // Off the left side, don't draw!
    }
    
    // Get light level
    const Fixed yScale = fixedMul(xScale, gStretch);    // Adjust for aspect ratio
    pVisSprite->yscale = yScale;
    pVisSprite->pSprite = pSpriteFrameAngle;
    pVisSprite->x1 = x1;                                // Save the edge coords
    pVisSprite->x2 = x2;
    pVisSprite->thing = &thing;

    if (thing.flags & MF_SHADOW) {                          // Draw a shadow...
        x1 = 0x8000U;
    } else if (pStatePtr->SpriteFrame & FF_FULLBRIGHT) {
        x1 = 255;                                           // full bright
    } else {
        x1 = thing.subsector->sector->lightlevel;           // + extralight;

        if ((uint32_t) x1 >= 256) {
            x1 = 255;                                       // Use maximum
        }
    }

    if (pSpriteFrameAngle->flipped != 0) {  // Reverse the shape?
        x1 |= 0x4000;
    }

    pVisSprite->colormap = x1;                                      // Save the light value
    tz = thing.z - gViewZ;
    tz += (((Fixed) pSpriteFrameAngle->topOffset) << FRACBITS);     // Height offset

    // Determine screen top and bottom Y for the sprite
    const Fixed topY = (gCenterY << FRACBITS) - fixedMul(tz, yScale);
    const Fixed botY = topY + fixedMul(pSpriteFrameAngle->height << FRACBITS, yScale);
    pVisSprite->y1 = topY >> FRACBITS;
    pVisSprite->y2 = botY >> FRACBITS;

    // Check if vertically offscreen, if not use the sprite record
    if (pVisSprite->y2 >= 0 || pVisSprite->y1 < (int) gScreenHeight) {
        gpEndVisSprite = pVisSprite + 1;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Given a sector pointer, and if I hadn't already rendered the sprites, make valid sprites for the sprite list.
//----------------------------------------------------------------------------------------------------------------------
static void addSectorSpritesToFrame(sector_t& sector) noexcept {    
    if (sector.validcount != gValidCount) {     // Has this been processed?
        sector.validcount = gValidCount;        // Mark it           
        mobj_t* pThing = sector.thinglist;      // Init the thing list

        while (pThing) {                        // Traverse the linked list
            addMapObjToFrame(*pThing);          // Draw the object if ok...
            pThing = pThing->snext;             // Next?
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Store the data describing a wall section that needs to be drawn
//----------------------------------------------------------------------------------------------------------------------
static void storeWallRange(
    const uint32_t leftX, 
    const uint32_t rightX,
    const seg_t& curLine,
    const angle_t lineAngle
) noexcept {
    wallPrep(leftX, rightX, curLine, lineAngle);    // Create the wall data
}

//----------------------------------------------------------------------------------------------------------------------
// Clips a wall segment and adds it to the solid wall segment list for masking.
//----------------------------------------------------------------------------------------------------------------------
static void clipSolidWallSegment(
    const int32_t leftX,
    const int32_t rightX,
    const seg_t& curLine,
    const angle_t lineAngle
) noexcept {
    // Init start table
    cliprange_t* pStartCR = gSolidsegs;
    
    // Find the first range that touches the range (adjacent pixels are touching)
    {
        const int32_t temp = leftX - 1;
        while (pStartCR->rightX < temp) {      // Loop down
            ++pStartCR;                        // Next entry
        }
    }

    if (leftX < pStartCR->leftX) {                                  // Clipped on the left?
        if (rightX < pStartCR->leftX - 1) {                         // post is entirely visible, so insert a new clippost
            storeWallRange(leftX, rightX, curLine, lineAngle);      // Draw the wall
            cliprange_t* pNextCR = gNewEnd;
            gNewEnd = pNextCR + 1;                                  // Save the new last entry

            while (pNextCR != pStartCR) {                           // Copy the current entry over
                --pNextCR;                                          // Move back one
                pNextCR[1] = pNextCR[0];                            // Copy the struct
            }

            pStartCR->leftX = leftX;                                // Insert the new record
            pStartCR->rightX = rightX;
            return;                                                 // Exit now
        }

        // Oh oh, there is a wall in front, clip me      
        storeWallRange(leftX, pStartCR->leftX - 1, curLine, lineAngle);     // I am clipped on the right
        pStartCR->leftX = leftX;                                            // Adjust the clip size to a new left edge
    }

    if (rightX <= pStartCR->rightX) {       // Is the rest obscured?
        return;                             // Yep, exit now
    }

    // Start has the first post group that needs to be removed
    cliprange_t* pNextCR = pStartCR;
    cliprange_t* pNextCR2 = pNextCR + 1;

    while (rightX >= pNextCR2->leftX - 1) {
        // There is a fragment between two posts
        storeWallRange(pNextCR->rightX + 1, pNextCR2->leftX - 1, curLine, lineAngle);
        pNextCR = pNextCR2;
        if (rightX <= pNextCR2->rightX) {           // bottom is contained in next
            pStartCR->rightX = pNextCR2->rightX;    // adjust the clip size
            goto crunch;                            // Don't store the final fragment
        }
        ++pNextCR2;
    }

    storeWallRange(pNextCR->rightX + 1, rightX, curLine, lineAngle);    // Save the final fragment
    pStartCR->rightX = rightX;                                          // Adjust the clip size (Clipped on the left)
    
crunch:
    // Remove start + 1 to next from the clip list, because start now covers their area
    if (pNextCR != pStartCR) {      // Do I need to remove any?
        if (pNextCR != gNewEnd) {   // remove a post
            do {
                ++pNextCR;
                ++pStartCR;
                pStartCR[0] = pNextCR[0];   // Copy the struct
            } while (pNextCR != gNewEnd);
        }
        gNewEnd = pStartCR + 1;             // All disposed!
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Clips a wall segment but does not add it to the solid wall segment list for masking.
//----------------------------------------------------------------------------------------------------------------------
static void clipPassWallSegment(
    const int32_t leftX,
    const int32_t rightX,
    const seg_t& curLine,
    const angle_t lineAngle
) noexcept {
    // Find the first range that touches the range (adjacent pixels are touching)
    cliprange_t *pClipRange = gSolidsegs;

    {
        const int32_t temp = leftX - 1;         // Leftmost edge I can ignore
        while (pClipRange->rightX < temp) {     // Skip over non-touching posts
            ++pClipRange;                       // Next index    
        }
    }

    if (leftX < pClipRange->leftX) {                                            // Is the left side visible?
        if (rightX < pClipRange->leftX - 1) {                                   // Post is entirely visible (above start)
            storeWallRange(leftX, rightX, curLine, lineAngle);                  // Store the range!
            return;                                                             // Exit now!
        }
        storeWallRange(leftX, pClipRange->leftX - 1, curLine, lineAngle);       // Oh oh, I clipped on the right!
    }
    
    // At this point, I know that some leftmost pixels are hidden.
    if (rightX <= pClipRange->rightX) {     // All are hidden?
        return;                             // Don't draw.
    }
    
    // Now draw all fragments behind solid posts
    cliprange_t* pNextClipRange = pClipRange + 1;   // Next index

    while (rightX >= pNextClipRange->leftX - 1) {
        storeWallRange(pClipRange->rightX + 1, pNextClipRange->leftX - 1, curLine, lineAngle);
        if (rightX <= pNextClipRange->rightX) {     // Is the rest hidden?
            return;
        }
        pClipRange = pNextClipRange;    // Next index
        ++pNextClipRange;               // Inc running pointer
    }

    // Draw the final fragment
    storeWallRange(pClipRange->rightX + 1, rightX, curLine, lineAngle);
}

//----------------------------------------------------------------------------------------------------------------------
// Clips the given segment and adds any visible pieces to the line list.
// I also add to the solid wall list so that I can rule out BSP sections quickly.
//----------------------------------------------------------------------------------------------------------------------
static void addLineToFrame(seg_t& line, sector_t& frontSector) noexcept {
    angle_t angle1 = PointToAngle(gViewX, gViewY, line.v1.x, line.v1.y);    // Calc the angle for the left edge
    angle_t angle2 = PointToAngle(gViewX, gViewY, line.v2.x, line.v2.y);    // Now the right edge

    const angle_t span = angle1 - angle2;   // Get the line span
    if (span >= ANG180) {                   // Backwards?
        return;                             // Don't handle backwards lines
    }

    const angle_t lineAngle = angle1;       // Store the leftmost angle for StoreWallRange
    angle1 -= gViewAngle;                   // Adjust the angle for viewangle
    angle2 -= gViewAngle;

    angle_t tspan = angle1 + gClipAngle;    // Adjust the center x of 0
    if (tspan > gDoubleClipAngle) {         // Possibly off the left side?
        tspan -= gDoubleClipAngle;          // See if it's visible
        if (tspan >= span) {                // Off the left?
            return;                         // Remove it
        }
        angle1 = gClipAngle;                // Clip the left edge
    }

    tspan = gClipAngle - angle2;            // Get the right edge adjustment
    if (tspan > gDoubleClipAngle) {         // Possibly off the right side?
        tspan -= gDoubleClipAngle;
        if (tspan >= span) {                // Off the right?
            return;                         // Off the right side
        }
        angle2 = -(int32_t) gClipAngle;     // Clip the right side
    }

    // The seg is in the view range, but not necessarily visible.
    // It may be a line for specials or imbedded floor line.
    angle1 = (angle1 + ANG90) >> (ANGLETOFINESHIFT + 1);        // Convert angles to table indexs
    angle2 = (angle2 + ANG90) >> (ANGLETOFINESHIFT + 1);
    angle1 = gViewAngleToX[angle1];                             // Get the screen x left
    angle2 = gViewAngleToX[angle2];                             // Screen x right

    if (angle1 >= angle2) {
        return;     // This is too small to bother with or invalid
    }

    --angle2;                                           // Make the right side inclusive
    sector_t* const pBackSector = line.backsector;      // Get the back sector

    if ((!pBackSector) ||                                               // Single sided line?
        (pBackSector->ceilingheight <= frontSector.floorheight) ||      // Closed door?
        (pBackSector->floorheight >= frontSector.ceilingheight)
    ) {
        clipSolidWallSegment(angle1, angle2, line, lineAngle);          // Make a SOLID wall
        return;
    }

    if ((pBackSector->ceilingheight != frontSector.ceilingheight) ||    // Normal window
        (pBackSector->floorheight != frontSector.floorheight) ||
        (pBackSector->CeilingPic != frontSector.CeilingPic) ||          // Different texture
        (pBackSector->FloorPic != frontSector.FloorPic) ||              // Floor texture
        (pBackSector->lightlevel != frontSector.lightlevel) ||          // Differant light?
        line.sidedef->midtexture                                        // Center wall texture?
    ) {
        clipPassWallSegment(angle1, angle2, line, lineAngle);           // Render but allow walls behind it
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Given a subsector pointer, pass all walls to the rendering engine. Also pass all the sprites.
//----------------------------------------------------------------------------------------------------------------------
static void addSubsectorToFrame(subsector_t& sub) noexcept {
    sector_t& sector = *sub.sector;     // Get the front sector  
    addSectorSpritesToFrame(sector);    // Prepare sprites for rendering
    
    seg_t* pLineSeg = sub.firstline;
    seg_t* const pEndLineSeg = pLineSeg + sub.numsublines;

    while (pLineSeg < pEndLineSeg) {
        addLineToFrame(*pLineSeg, sector);      // Render each line
        ++pLineSeg;                             // Inc the line pointer
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Check if any part of the BSP bounding box is touching the view arc. 
// Also project the width of the box to screen coords to see if it is too small to even bother with.
// If I should process this box then return 'true'.
//----------------------------------------------------------------------------------------------------------------------
static bool checkBBox(const Fixed bspcoord[BOXCOUNT]) noexcept {
    // Left and right angles for view
    angle_t angle1;
    angle_t angle2;

    // Find the corners of the box that define the edges from current viewpoint.
    // First use the box pointer:
    {
        uint32_t* pBox = &gCheckCoord[0][0];        // Init to the base of the table (Above)
        if (gViewY < bspcoord[BOXTOP]) {            // Off the top?
            pBox += 12;                             // Index to center
            if (gViewY <= bspcoord[BOXBOTTOM]) {    // Off the bottom?
                pBox += 12;                         // Index to below
            }
        }

        if (gViewX > bspcoord[BOXLEFT]) {           // Check if off the left edge
            pBox += 4;                              // Center x
            if (gViewX >= bspcoord[BOXRIGHT]) {     // Is it off the right?
                pBox += 4;
            }
        }

        if (pBox[0] == -1) {    // Center node?
            return true;        // I am in the center of the box, process it!!
        }
        
        // I now have in 3 Space the endpoints of the BSP box, now project it to the screen
        // and see if it is either off the screen or too small to even care about
        angle1 = PointToAngle(gViewX, gViewY, bspcoord[pBox[0]], bspcoord[pBox[1]]) - gViewAngle;   // What is the projected angle?
        angle2 = PointToAngle(gViewX, gViewY, bspcoord[pBox[2]], bspcoord[pBox[3]]) - gViewAngle;   // Now the rightmost angle
    }

    // Use span and tspan
    {
        const angle_t span = angle1 - angle2;   // What is the span of the angle?
        if (span >= ANG180) {                   // Whoa... I must be sitting on the line or it's in my face!
            return true;                        // Process this one...
        }
    
        // angle1 must be treated as signed, so to see if it is either >-clipangle and < clipangle
        // I add clipangle to the angle to adjust the 0 center and compare to clipangle * 2
    
        angle_t tspan = angle1 + gClipAngle;
        if (tspan > gDoubleClipAngle) {         // Possibly off the left edge
            tspan -= gDoubleClipAngle;
            if (tspan >= span) {                // Off the left side?
                return false;                   // Don't bother, it's off the left side
            }
            angle1 = gClipAngle;                // Clip the left edge
        }
    
        tspan = gClipAngle - angle2;            // Move from a zero base of "clipangle"
        if (tspan > gDoubleClipAngle) {         // Possible off the right edge
            tspan -= gDoubleClipAngle;
            if (tspan >= span) {                // The entire span is off the right edge?
                return false;                   // Too far right!
            }
            angle2 = -(int32_t) gClipAngle;     // Clip the right edge angle
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

    return true;    // Process me!
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
    
    // Decide which side the view point is on
    uint32_t side = PointOnVectorSide(gViewX, gViewY, &pNode->Line);    // Is this the front side?
    addBspNodeToFrame((const node_t*) pNode->Children[side]);           // Process the side closer to me
    side ^= 1;                                                          // Swap the side
    
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
    gSolidsegs[0].leftX = -0x4000;          // Fake leftmost post
    gSolidsegs[0].rightX = -1;
    gSolidsegs[1].leftX = gScreenWidth;     // Fake rightmost post
    gSolidsegs[1].rightX = 0x4000;
    gNewEnd = gSolidsegs + 2;               // Init the free memory pointer
    addBspNodeToFrame(gpBSPTreeRoot);       // Begin traversing the BSP tree for all walls in render range
}

END_NAMESPACE(Renderer)
