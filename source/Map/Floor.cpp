#include "Floor.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Change.h"
#include "Game/Tick.h"
#include "GFX/Textures.h"
#include "MapData.h"
#include "Specials.h"

// Describes a moving floor
struct floormove_t {
    sector_t*   sector;             // Sector connected to
    Fixed       floordestheight;    // Destination height
    Fixed       speed;              // Speed of motion
    uint32_t    newspecial;         // Special event number
    uint32_t    texture;            // Texture when at the bottom
    int         direction;          // Direction of travel
    floor_e     type;               // Type of floor
    bool        crush;              // Can it crush you?
};

static constexpr Fixed FLOORSPEED = FRACUNIT * 3;    // Standard floor motion speed

//----------------------------------------------------------------------------------------------------------------------
// Move a plane (floor or ceiling) and check for crushing Called by Doors.c, Plats.c, Ceilng.c and Floor.c.
// If ChangeSector returns true, then something is obstructing the moving platform and it has to stop or bounce back.
//----------------------------------------------------------------------------------------------------------------------
result_e T_MovePlane(
    sector_t& sector,
    const Fixed speed,
    const Fixed dest,
    const bool bCrush,
    const bool bCeiling,
    const int32_t direction
) noexcept {
    Fixed lastpos;      // Previous height

    if (!bCeiling) {
        lastpos = sector.floorheight;                       // Save the previous height
        if (direction == -1) {                              // Down
            if (lastpos - speed < dest) {                   // Moved too fast?
                sector.floorheight = dest;                  // Set the new height
                if (ChangeSector(sector, bCrush)) {         // Set the new plane
                    sector.floorheight = lastpos;           // Restore the height
                    ChangeSector(sector, bCrush);           // Put if back
                }
                return pastdest;                            // Went too far! (Stop motion)
            } else {
                sector.floorheight = lastpos-speed;         // Adjust the motion
                if (ChangeSector(sector, bCrush)) {         // Place it
                    sector.floorheight = lastpos;           // Put it back
                    ChangeSector(sector, bCrush);           // Crush...
                    return crushed;                         // Squishy!!
                }
            }
        } else if (direction == 1) {                        // Up
            if (lastpos + speed > dest) {
                sector.floorheight = dest;
                if (ChangeSector(sector, bCrush)) {         // Try to move it
                    sector.floorheight = lastpos;           // Put it back!
                    ChangeSector(sector, bCrush);
                }
                return pastdest;                            // I got to the end...
            } else {                                        // Could get crushed
                sector.floorheight = lastpos + speed;
                if (ChangeSector(sector, bCrush)) {
                    if (!bCrush) {                          // If it can't crush, put it back
                        sector.floorheight = lastpos;       // Mark
                        ChangeSector(sector, bCrush);       // Put it back
                    }
                    return crushed;
                }
            }
        }
    } else {                                                // Ceiling
        lastpos = sector.ceilingheight;
        if (direction == -1) {                              // Down
            if (lastpos - speed < dest) {
                sector.ceilingheight = dest;
                if (ChangeSector(sector, bCrush)) {
                    sector.ceilingheight = lastpos;
                    ChangeSector(sector, bCrush);
                }
                return pastdest;
            } else {                                        // Could get crushed
                sector.ceilingheight = lastpos-speed;
                if (ChangeSector(sector, bCrush)) {
                    if (!bCrush) {
                        sector.ceilingheight = lastpos;
                        ChangeSector(sector, bCrush);
                    }
                    return crushed;
                }
            }
        } else if (direction == 1) {                        // Up
            if (lastpos + speed > dest) {                   // Moved too far?
                sector.ceilingheight = dest;                // Set the dest
                if (ChangeSector(sector, bCrush)) {
                    sector.ceilingheight = lastpos;         // Restore it
                    ChangeSector(sector, bCrush);
                }
                return pastdest;
            } else {
                sector.ceilingheight = lastpos+speed;       // Move it
                ChangeSector(sector, bCrush);               // Squish if needed
            }
        }
    }
    return moveok;
}

//----------------------------------------------------------------------------------------------------------------------
// Move a floor to it's destination (Up or down)
// This is a thinker function.
//----------------------------------------------------------------------------------------------------------------------
static void T_MoveFloor(floormove_t& floor) noexcept {
    // Do the move
    const result_e res = T_MovePlane(
        *floor.sector,
        floor.speed,
        floor.floordestheight,
        floor.crush,
        false,
        floor.direction
    );

    if (gbTick4) {  // Time for a sound?
        S_StartSound(&floor.sector->SoundX, sfx_stnmov);
    }

    if (res == pastdest) {                      // Floor reached it's destination?
        floor.sector->specialdata = nullptr;    // Remove the floor
        if (floor.direction == 1) {             // Which way?
            if (floor.type == donutRaise) {
                floor.sector->special = floor.newspecial;   // Set the special type
                floor.sector->FloorPic = floor.texture;     // Set the new texture
            }
        } else if (floor.direction == -1) {
            if (floor.type == lowerAndChange) {
                floor.sector->special = floor.newspecial;
                floor.sector->FloorPic = floor.texture;
            }
        }

        RemoveThinker(floor);   // Remove the floor record
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Create a moving floor
//----------------------------------------------------------------------------------------------------------------------
bool EV_DoFloor(line_t& line, const floor_e floortype) noexcept {
    bool rtn = false;                   // Assume no entry
    uint32_t secnum = UINT32_MAX;       // Sector number

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) != UINT32_MAX) {
        // Already moving?  If so, keep going...
        sector_t& sec = gpSectors[secnum];
        if (sec.specialdata) {  // Already has a floor attached?
            continue;
        }

        // New floor thinker
        rtn = true;                                         // I created a floor
        floormove_t& floor = AddThinker(T_MoveFloor);
        sec.specialdata = &floor;                           // Mark the sector
        floor.type = floortype;                             // Save the type of floor
        floor.crush = false;                                // Assume it can't crush
        floor.sector = &sec;                                // Set the current sector
        floor.speed = FLOORSPEED;                           // Assume normal speed

        // Handle all the special cases
        switch (floortype) {
            case lowerFloor:
                floor.floordestheight = P_FindHighestFloorSurrounding(sec);
                floor.direction = -1;   // Go down
                break;

            case lowerFloorToLowest:
                floor.floordestheight = P_FindLowestFloorSurrounding(sec);
                floor.direction = -1;   // Go down
                break;

            case turboLower:
                floor.floordestheight = (8 * FRACUNIT) + P_FindHighestFloorSurrounding(sec);
                floor.speed = FLOORSPEED * 4;   // Fast speed
                floor.direction = -1;           // Go down
                break;

            case raiseFloorCrush:
                floor.crush = true;     // Enable crushing
            case raiseFloor: {
                floor.direction = 1;    // Go up
                floor.floordestheight = P_FindLowestCeilingSurrounding(sec);
                if (floor.floordestheight > sec.ceilingheight) {    // Too high?
                    floor.floordestheight = sec.ceilingheight;      // Set maximum
                }
            }   break;

            case raiseFloorToNearest: {
                floor.direction = 1;    // Go up
                floor.floordestheight = P_FindNextHighestFloor(sec, sec.floorheight);
            }   break;

            case raiseFloor24AndChange: // Raise 24 pixels and change texture
                sec.FloorPic = line.frontsector->FloorPic;
                sec.special = line.frontsector->special;
            case raiseFloor24:          // Just raise 24 pixels
                floor.direction = 1;    // Go up
                floor.floordestheight = floor.sector->floorheight + (24<<FRACBITS);
                break;

            case raiseToTexture: {
                floor.direction = 1;
                uint32_t i = 0;
                uint32_t minsize = 32767U;       // Maximum height

                while (i < sec.linecount) {
                    if (twoSided(sec, i)) {                     // Only process two sided lines
                        side_t* pSide = &getSide(sec, i, 0);    // Get the first side

                        if ((pSide->bottomtexture & 0x8000) == 0) { // Valid texture?
                            const Texture* const pBottomTex = Textures::getWall(pSide->bottomtexture);
                            const uint32_t height = pBottomTex->data.height;

                            if (height < minsize) {
                                minsize = height;
                            }
                        }

                        pSide = &getSide(sec, i, 1);    // Get the second side

                        if ((pSide->bottomtexture & 0x8000) == 0) { // Valid texture?
                            const Texture* const pBottomTex = Textures::getWall(pSide->bottomtexture);
                            const uint32_t height = pBottomTex->data.height;

                            if (height < minsize) {
                                minsize = height;
                            }
                        }
                    }
                    ++i;        // Next count
                }
                floor.floordestheight = floor.sector->floorheight + ((Fixed) minsize << FRACBITS);  // Set the height
            }   break;

            case lowerAndChange: {
                floor.direction = -1;
                floor.floordestheight = P_FindLowestFloorSurrounding(sec);
                floor.texture = sec.FloorPic;
                uint32_t i = 0;

                while (i < sec.linecount) {
                    // Only process two sided lines
                    if (twoSided(sec, i)) {
                        // Get the opposite sector
                        sector_t& opSec = (getSide(sec, i, 0).sector == &sec) ? getSector(sec, i, 1) : getSector(sec, i, 0);

                        // Only use this sector if it matches our destination floor height
                        if (opSec.floorheight == floor.floordestheight) {
                            floor.texture = opSec.FloorPic;
                            floor.newspecial = opSec.special;
                            break;
                        }
                    }
                    ++i;
                }
            }   break;

            case donutRaise:    // DC: Unhandled case
                break;
        }
    }
    return rtn;
}

//----------------------------------------------------------------------------------------------------------------------
// Build a staircase!
//----------------------------------------------------------------------------------------------------------------------
bool EV_BuildStairs(line_t& line) noexcept {
    uint32_t secnum = UINT32_MAX;
    bool rtn = false;   // Assume no thinkers made

    while ((secnum = P_FindSectorFromLineTag(line,secnum)) != UINT32_MAX) {
        // Already moving? If so, try another one
        sector_t& sec = gpSectors[secnum];
        if (sec.specialdata) {
            continue;
        }

        // New floor thinker
        rtn = true;
        Fixed height = sec.floorheight + (8 << FRACBITS);     // Go up 8 pixels

        floormove_t& floor1 = AddThinker(T_MoveFloor);
        sec.specialdata = &floor1;          // Attach the record
        floor1.direction = 1;               // Move up
        floor1.sector = &sec;               // Set the proper sector
        floor1.speed = FLOORSPEED / 2;      // Normal speed
        floor1.floordestheight = height;    // Set the new height

        // Loop vars
        const uint32_t texture = sec.FloorPic;      // Cache the texture for the stairs
        bool bStay;                                 // Flag to break the stair building loop

        // Find next sector to raise
        // 1. Find 2-sided line with same sector side[0]
        // 2. Other side is the next sector to raise
        sector_t* pCurSec = &sec;

        do {
            bStay = false;  // Assume I fall out initially
            
            for (uint32_t i = 0; i < pCurSec->linecount; ++i) {
                if (!twoSided(*pCurSec, i)) {    // Two sided line?
                    continue;
                }

                sector_t *tsec = pCurSec->lines[i]->frontsector;    // Is this the sector?
                if (pCurSec != tsec) {                              // Not a match!
                    continue;
                }

                tsec = pCurSec->lines[i]->backsector;   // Get the possible dest
                if (tsec->FloorPic != texture) {        // Not the same texture?
                    continue;
                }

                height += (8 << FRACBITS);      // Increase the height
                if (tsec->specialdata != 0) {   // Busy already?
                    continue;
                }
                
                pCurSec = tsec;     // I continue from here 

                floormove_t& floor2 = AddThinker(T_MoveFloor);

                pCurSec->specialdata = &floor2;     // Attach this floor
                floor2.direction = 1;               // Go up
                floor2.sector = pCurSec;            // Set the sector I will affect
                floor2.speed = FLOORSPEED / 2;      // Slow speed
                floor2.floordestheight = height;
                bStay = true;                       // I linked to a sector                
                break;                              // Restart the loop
            }
        } while (bStay);
    }

    return rtn;     // Did I do it?
}

//----------------------------------------------------------------------------------------------------------------------
// Create a moving floor in the form of a donut
//----------------------------------------------------------------------------------------------------------------------
bool EV_DoDonut(line_t& line) noexcept {
    uint32_t secnum = UINT32_MAX;
    bool rtn = false;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) != UINT32_MAX) {
        // Already moving?  if so, keep going...
        sector_t& s1 = gpSectors[secnum];
        if (s1.specialdata) {
            continue;
        }

        // DC: Added check here to make sure back sector exists!
        sector_t* pS2 = getNextSector(*s1.lines[0], s1);        
        if (!pS2) {
            continue;
        }

        rtn = true;
        sector_t& s2 = *pS2;
        const uint32_t lineCount = s2.linecount;

        for (uint32_t i = 0; i < lineCount; ++i) {
            // Must have a back sector
            if ((s2.lines[i]->flags & ML_TWOSIDED) == 0) {
                continue;
            }

            if (s2.lines[i]->backsector == &s1) {
                continue;
            }

            // Get the back sector
            sector_t& s3 = *s2.lines[i]->backsector;

            // Spawn rising slime
            floormove_t& floor1 = AddThinker(T_MoveFloor);
            s2.specialdata = &floor1;
            floor1.type = donutRaise;
            floor1.crush = false;
            floor1.direction = 1;   // Going up
            floor1.sector = &s2;
            floor1.speed = FLOORSPEED / 2;
            floor1.texture = s3.FloorPic;
            floor1.newspecial = 0;
            floor1.floordestheight = s3.floorheight;

            // Spawn lowering donut-hole
            floormove_t& floor2 = AddThinker(T_MoveFloor);
            s1.specialdata = floor;
            floor2.type = lowerFloor;
            floor2.crush = false;
            floor2.direction = -1;  // Going down
            floor2.sector = &s1;
            floor2.speed = FLOORSPEED / 2;
            floor2.floordestheight = s3.floorheight;
            break;
        }
    }

    return rtn;
}
