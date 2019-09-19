#include "MapUtil.h"

#include "Base/Tables.h"
#include "Game/Data.h"
#include "MapData.h"
#include "Things/MapObj.h"

//----------------------------------------------------------------------------------------------------------------------
// Given the numerator and the denominator of a fraction for a slope, return the equivalent angle.
// Note: I assume that denominator is greater or equal to the numerator.
//----------------------------------------------------------------------------------------------------------------------
angle_t SlopeAngle(uint32_t num, uint32_t den) noexcept {
    num >>= (FRACBITS - 3);                     // Leave in 3 extra bits for just a little more precision
    den >>= FRACBITS;                           // Must be an int
    num = num * (gIDivTable[den] >> 9);         // Perform the divide using a recipocal mul
    num >>= ((FRACBITS + 3) - SLOPEBITS);       // Isolate the fraction for index to the angle table

    if (num > SLOPERANGE) {     // Out of range?
        num = SLOPERANGE;       // Fix it
    }

    return gTanToAngle[num];    // Get the angle
}

static inline angle_t SlopeAngle(int32_t num, int32_t den) noexcept {
    ASSERT(num >= 0);
    ASSERT(den >= 0);
    return SlopeAngle((uint32_t) num, (uint32_t) den);
}

//----------------------------------------------------------------------------------------------------------------------
// To get a global angle from cartesian coordinates, the coordinates are flipped until they are in the
// first octant of the coordinate system, then the y (<=x) is scaled and divided by x to get a
// tangent (slope) value which is looked up in the tantoangle[] table.
//----------------------------------------------------------------------------------------------------------------------
angle_t PointToAngle(Fixed x1, Fixed y1, Fixed x2, Fixed y2) noexcept {
    x2 -= x1;   // Convert the two points into a fractional slope
    y2 -= y1;

    if (x2 != 0 || y2 != 0) {                                   // Not 0,0?
        if (x2 >= 0) {                                          // Positive x?
            if (y2 >= 0) {                                      // Positive x, Positive y
                if (x2 > y2) {                                  // Octant 0?
                    return SlopeAngle(y2, x2);                  // Octant 0
                }
                return (ANG90 - 1) - SlopeAngle(x2, y2);        // Octant 1
            }
            y2 = -y2;                                           // Get the absolute value of y (Was negative)
            if (x2 > y2) {                                      // Octant 6
                return negateAngle(SlopeAngle(y2, x2));         // Octant 6
            }
            return SlopeAngle(x2, y2) + ANG270;                 // Octant 7
        }
        x2 = -x2;                                               // Negate x (Make it positive)
        if (y2 >= 0) {                                          // Positive y?
            if (x2 > y2) {                                      // Octant 3?
                return (ANG180 - 1) - SlopeAngle(y2, x2);       // Octant 3
            }
            return SlopeAngle(x2, y2) + ANG90;                  // Octant 2
        }
        y2 = -y2;                                               // Negate y (Make it positive)
        if (x2 > y2) {
            return SlopeAngle(y2 , x2) + ANG180;                // Octant 4
        }
        return (ANG270 - 1) - SlopeAngle(x2, y2);               // Octant 5
    }
    return 0;                                                   // In case of 0,0, return an angle of 0
}

//----------------------------------------------------------------------------------------------------------------------
// Gives an estimation of distance (not exact).
// This is used when an exact distance between two points is not needed.
//----------------------------------------------------------------------------------------------------------------------
Fixed GetApproxDistance(Fixed dx, Fixed dy) noexcept {
    if (dx < 0) {
        dx = -dx;       // Get the absolute value of the distance
    }
    
    if (dy < 0) {
        dy = -dy;
    }

    if (dx < dy) {      // Is the x smaller?
        dx >>= 1;       // Use half the x
    } else {
        dy >>= 1;       // Or use half the y
    }
    
    dx += dy;           // Add larger and half of the smaller for distance
    return dx;          // Return result
}

//----------------------------------------------------------------------------------------------------------------------
// Return TRUE if the point is on the BACK side.
// Otherwise return FALSE if on the front side.
// This code is optimized for the simple case of vertical or horizontal lines.
//----------------------------------------------------------------------------------------------------------------------
bool PointOnVectorSide(Fixed x, Fixed y, const vector_t& line) noexcept {
    // Assume I am on the back side initially
    bool bResult = true;

    //------------------------------------------------------------------------------------------------------------------
    // Special case #1, vertical lines
    //------------------------------------------------------------------------------------------------------------------
    Fixed dx = line.dx;     // Cache the line vector's delta x and y
    Fixed dy = line.dy;
    x = x - line.x;         // Get the offset from the base of the line

    if (dx == 0) {          // Vertical line? (dy is base direction)
        if (x <= 0) {       // Which side of the line am I?
            dy = -dy;
        }
        if (dy >= 0) {
            bResult = false;    // On the front side!
        }
        return bResult;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Special case #2, horizontal lines
    //------------------------------------------------------------------------------------------------------------------
    y = y - line.y;         // Get the offset for y
    if (dy == 0) {          // Horizontal line? (dx is base direction)
        if (y <= 0) {       // Which side of the line
            dx = -dx;
        }
        if (dx <= 0) {
            bResult = false;    // On the front side!
        }
        return bResult;         // Return the answer
    }

    //------------------------------------------------------------------------------------------------------------------
    // Special case #3, Sign compares
    //------------------------------------------------------------------------------------------------------------------
    if (((uint32_t)(dy ^ dx ^ x ^ y) & 0x80000000UL) != 0) {    // Negative compound sign?
        if (((uint32_t)(dy ^ x) & 0x80000000UL) == 0) {         // Positive cross product?
            bResult = false;                                    // Front side is positive
        }
        return bResult;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Case #4, do it the hard way with a cross product
    //------------------------------------------------------------------------------------------------------------------
    x >>= FRACBITS;             // Get the integer
    y >>= FRACBITS;
    x = (dy >> FRACBITS) * x;   // Create the cross product
    y = (dx >> FRACBITS) * y;

    if (y < x) {            // Which side?
        bResult = false;    // Front side
    }

    return bResult;         // Return the side
}

//----------------------------------------------------------------------------------------------------------------------
// Return the pointer to a subsector record using an input x and y.
// Uses the BSP tree to assist.
//----------------------------------------------------------------------------------------------------------------------
subsector_t& PointInSubsector(const Fixed x, const Fixed y) noexcept {
    // Note: there is ALWAYS a BSP tree - no checks needed on loop start!
    ASSERT(gpBSPTreeRoot);
    node_t* pNode = gpBSPTreeRoot;

    while (true) {
        // Goto the child on the side of the split that the point is on.
        // Stop the loop when we encounter a subsector child:
        const uint32_t sidePointIsOn = PointOnVectorSide(x, y, pNode->Line);
        pNode = (node_t*) pNode->Children[sidePointIsOn];

        if (isBspNodeASubSector(pNode))
            break;
    }

    return *((subsector_t*) getActualBspNodePtr(pNode));    // N.B: Pointer needs flag removed via this!
}

//----------------------------------------------------------------------------------------------------------------------
// Convert a line record into a line vector
//----------------------------------------------------------------------------------------------------------------------
void MakeVector(line_t& li, vector_t& dl) noexcept {
    Fixed temp = li.v1.x;       // Get the X of the vector
    dl.x = temp;                // Save the x
    dl.dx = li.v2.x - temp;     // Get the X delta

    // Do the same for the Y
    temp = li.v1.y;
    dl.y = temp;
    dl.dy = li.v2.y - temp;
}

//----------------------------------------------------------------------------------------------------------------------
// Returns the fractional intercept point along the first vector
//----------------------------------------------------------------------------------------------------------------------
Fixed InterceptVector(const vector_t& first, const vector_t& second) noexcept {
    const Fixed dx2 = second.dx >> FRACBITS;        // Get the integer of the second line
    const Fixed dy2 = second.dy >> FRACBITS;
    Fixed den = dy2 * (first.dx >> FRACBITS);       // Get the step slope of the first vector
    den -= dx2 * (first.dy >> FRACBITS);            // Sub the step slope of the second vector
    
    if (den == 0) {     // They are parallel vectors!
        return -1;      // Infinity
    }

    Fixed num = ((second.x - first.x) >> FRACBITS) * dy2;   // Get the slope to the start position
    num += ((first.y - second.y) >> FRACBITS) * dx2;
    num <<= FRACBITS;                                       // Convert to fixed point
    num = num / den;                                        // How far to the intersection?
    return num;                                             // Return the distance of intercept
}

//----------------------------------------------------------------------------------------------------------------------
// Return the height of the open span or zero if a single sided line.
//----------------------------------------------------------------------------------------------------------------------
uint32_t LineOpening(const line_t& linedef) noexcept {
    Fixed top = 0;
    const sector_t* const pBack = linedef.backsector;   // Get the back side

    if (pBack) {                                                // Double sided line!
        const sector_t* const pFront = linedef.frontsector;     // Get the front sector
        top = pFront->ceilingheight;                            // Assume front height
        if (top > pBack->ceilingheight) {
            top = pBack->ceilingheight;                         // Use the back height
        }

        Fixed bottom = pFront->floorheight;     // Assume front height
        if (bottom < pBack->floorheight) {
            bottom = pBack->floorheight;        // Use back sector height
        }
        
        top -= bottom;      // Get the span (Zero for a closed door)
    }

    return (uint32_t) top;  // Return the span
}

//----------------------------------------------------------------------------------------------------------------------
// Unlinks a thing from the block map and sectors
//----------------------------------------------------------------------------------------------------------------------
void UnsetThingPosition(mobj_t& thing) noexcept {
    // Things that have no shapes don't need to be in the sector map.
    // The sector links are used for drawing of the sprites.
    if ((thing.flags & MF_NOSECTOR) == 0) {
        // Item is visible!
        mobj_t* next = thing.snext;
        mobj_t* prev = thing.sprev;

        if (next) {                 // Is there a forward link?
            next->sprev = prev;     // Attach a new backward link
        }

        if (prev) {                 // Is there a backward link?
            prev->snext = next;     // Adjust the forward link
        } else {
            thing.subsector->sector->thinglist = next;      // New first link
        }
    }

    // Inert things don't need to be in blockmap such as missiles or blood and gore.
    // Those will have this flag set:
    if ((thing.flags & MF_NOBLOCKMAP) == 0) {
        mobj_t* next = thing.bnext;
        mobj_t* prev = thing.bprev;

        if (next) {     // Forward link?
            next->bprev = prev;
        }

        if (prev) {     // Is there a previous link?
            prev->bnext = next;
        } else {
            uint32_t blockx = (uint32_t)(thing.x - gBlockMapOriginX) >> MAPBLOCKSHIFT;  // Get the tile offsets
            uint32_t blocky = (uint32_t)(thing.y - gBlockMapOriginY) >> MAPBLOCKSHIFT;

            if (blockx < gBlockMapWidth && blocky < gBlockMapHeight) {  // Failsafe...
                blocky = blocky * gBlockMapWidth;
                blocky += blockx;
                gpBlockMapThingLists[blocky] = next;    // Index to the block map
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Links a thing into both a block and a subsector based on it's x & y.
// Sets thing->subsector properly.
//----------------------------------------------------------------------------------------------------------------------
void SetThingPosition(mobj_t& thing) noexcept {
    // Get the current subsector occupied and save a link to it on the thing
    subsector_t& ss = PointInSubsector(thing.x, thing.y);
    thing.subsector = &ss;

    // Invisible things don't go into the sector links so they don't draw
    if ((thing.flags & MF_NOSECTOR) == 0) {
        sector_t* const pSec = ss.sector;               // Get the current sector
        mobj_t* const pNextMObj = pSec->thinglist;      // Get the first link as my next
        thing.snext = pNextMObj;                        // Init the next link

        if (pNextMObj) {                    // Was there a link in the first place?
            pNextMObj->sprev = &thing;      // The previous is now here
        }

        thing.sprev = nullptr;          // Init the previous link
        pSec->thinglist = &thing;       // Set the new first link (Me!)
    }

    // Inert things don't need to be in blockmap, like blood and gore.
    // Those will have this flag set:
    if ((thing.flags & MF_NOBLOCKMAP) == 0) {
        thing.bprev = nullptr;      // No previous link
        thing.bnext = nullptr;

        const uint32_t blockx = (uint32_t)(thing.x - gBlockMapOriginX) >> MAPBLOCKSHIFT;    // Get the tile index
        const uint32_t blocky = (uint32_t)(thing.y - gBlockMapOriginY) >> MAPBLOCKSHIFT;

        if (blockx < gBlockMapWidth && blocky < gBlockMapHeight) {          // Failsafe
            const uint32_t blockIdx = blocky * gBlockMapWidth + blockx;
            mobj_t* const pNextMObj = gpBlockMapThingLists[blockIdx];       // Get the next link
            gpBlockMapThingLists[blockIdx] = &thing;                        // Set the new entry
            thing.bnext = pNextMObj;                                        // Save the new link

            if (pNextMObj) {                    // Is there a forward link?
                pNextMObj->bprev = &thing;      // Place a backward link
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// The validcount flags are used to avoid checking lines that are marked in multiple mapblocks,
// so increment validcount before the first call to BlockLinesIterator, then make one or more calls to it.
//----------------------------------------------------------------------------------------------------------------------
bool BlockLinesIterator(const uint32_t x, const uint32_t y, const BlockLinesIterCallback func) noexcept {
    if (x < gBlockMapWidth && y < gBlockMapHeight) {            // On the map?
        const uint32_t blockIdx = y * gBlockMapWidth + x;
        line_t** ppLineList = gpBlockMapLineLists[blockIdx];    // Get the first line pointer

        for (;;) {
            line_t* pLine = ppLineList[0];      // Get the entry
            if (!pLine) {                       // End of a list?
                break;                          // Get out of the loop
            }

            if (pLine->validCount != gValidCount) {     // Line not checked?
                pLine->validCount = gValidCount;        // Mark it
                if (!func(*pLine)) {                    // Call the line proc
                    return false;                       // I have a match?
                }
            }

            ++ppLineList;   // Next entry
        }
    }

    return true;    // Everything was checked
}

//----------------------------------------------------------------------------------------------------------------------
// Scan all objects standing on this map block.
//----------------------------------------------------------------------------------------------------------------------
bool BlockThingsIterator(const uint32_t x, const uint32_t y, const BlockThingsIterCallback func) noexcept {
    // Check if we are off the map or not
    if (x < gBlockMapWidth && y < gBlockMapHeight) {
        const uint32_t blockIdx = y * gBlockMapWidth + x;
        mobj_t* pMObj = gpBlockMapThingLists[blockIdx];     // Get the first object on the block

        // Continue iterating through this thing list
        while (pMObj) {
            // DC: Crash fix to the 3DO version: cache the next pointer first
            // as the callback might invalidate the map object!
            mobj_t* const pNextMObj = pMObj->bnext;

            if (!func(*pMObj)) {    // Call function
                return false;       // I found it!
            }

            pMObj = pNextMObj;  // Next object in list
        }
    }

    return true;    // Not found
}
