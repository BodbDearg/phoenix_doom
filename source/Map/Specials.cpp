#include "Specials.h"

#include "Base/Random.h"
#include "Ceiling.h"
#include "Doors.h"
#include "Floor.h"
#include "Game/Data.h"
#include "Game/DoomRez.h"
#include "Game/Game.h"
#include "Game/Tick.h"
#include "GFX/Textures.h"
#include "Lights.h"
#include "MapData.h"
#include "Platforms.h"
#include "Switch.h"
#include "Things/Interactions.h"
#include "Things/MapObj.h"
#include "Things/Teleport.h"

uint32_t gNumFlatAnims;     // Number of flat anims

anim_t gFlatAnims[] = {
    { rNUKAGE3 - rF_START, rNUKAGE1 - rF_START, rNUKAGE1 - rF_START },
    { rFWATER4 - rF_START, rFWATER1 - rF_START, rFWATER1 - rF_START },
    { rLAVA4 - rF_START, rLAVA1 - rF_START, rLAVA1 - rF_START }
};

static uint32_t     gNumLineSpecials;       // Number of line specials
static line_t**     gppLineSpecialList;     // Pointer to array of line pointers

//----------------------------------------------------------------------------------------------------------------------
// Init the picture animations for floor textures
//----------------------------------------------------------------------------------------------------------------------
void P_InitPicAnims() noexcept {
    gNumFlatAnims = sizeof(gFlatAnims) / sizeof(anim_t);    // Set the number
}

//----------------------------------------------------------------------------------------------------------------------
// Will return a side_t* given the number of the current sector, the line number, and the side (0/1) that you want.
//----------------------------------------------------------------------------------------------------------------------
side_t& getSide(sector_t& sec, const uint32_t line, const uint32_t side) noexcept {
    side_t* const pSide = sec.lines[line]->SidePtr[side];
    ASSERT(pSide);
    return *pSide;
}

//----------------------------------------------------------------------------------------------------------------------
// Will return a sector_t* given the number of the current sector, the line number and the side (0/1) that you want.
//----------------------------------------------------------------------------------------------------------------------
sector_t& getSector(sector_t& sec, const uint32_t line, const uint32_t side) noexcept {
    side_t* const pSide = sec.lines[line]->SidePtr[side];
    ASSERT(pSide->sector);
    return *pSide->sector;
}

//----------------------------------------------------------------------------------------------------------------------
// Given the sector pointer and the line number, will tell you whether the line is two-sided or not.
//----------------------------------------------------------------------------------------------------------------------
bool twoSided(sector_t& sec, const uint32_t line) noexcept {
    if ((sec.lines[line]->flags & ML_TWOSIDED) != 0) {
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// Return sector_t * of sector next to current. NULL if not two-sided line.
//----------------------------------------------------------------------------------------------------------------------
sector_t* getNextSector(line_t& line, sector_t& sec) noexcept {
    if ((line.flags & ML_TWOSIDED) == 0) {
        return nullptr;
    }

    if (line.frontsector == &sec) {     // Going backwards?
        return line.backsector;         // Go the other way
    } else {
        return line.frontsector;        // Use the front
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Find the lowest floor height in surrounding sectors
//----------------------------------------------------------------------------------------------------------------------
Fixed P_FindLowestFloorSurrounding(sector_t& sec) noexcept {
    Fixed floor = sec.floorheight;  // Get the current floor
    const uint32_t numLines = sec.linecount;
    line_t** const ppSectorLines = sec.lines;

    for (uint32_t i = 0; i < numLines; ++i) {
        ASSERT(ppSectorLines[i]);
        line_t& line = *ppSectorLines[i];
        const sector_t* const pOther = getNextSector(line, sec);
        if (pOther) {
            if (pOther->floorheight < floor) {      // Check the floor
                floor = pOther->floorheight;        // Lower floor
            }
        }
    }

    return floor;
}

//----------------------------------------------------------------------------------------------------------------------
// Find highest floor height in surrounding sectors
//----------------------------------------------------------------------------------------------------------------------
Fixed P_FindHighestFloorSurrounding(sector_t& sec) noexcept {
    Fixed floor = 0x80000000;   // Init to the lowest possible value
    const uint32_t numLines = sec.linecount;
    line_t** const ppSectorLines = sec.lines;

    for (uint32_t i = 0; i < numLines; ++i) {
        ASSERT(ppSectorLines[i]);
        line_t& line = *ppSectorLines[i];
        const sector_t* const pOther = getNextSector(line, sec);
        if (pOther) {
            if (pOther->floorheight > floor) {
                floor = pOther->floorheight;    // Get the new floor
            }
        }
    }

    return floor;
}

//----------------------------------------------------------------------------------------------------------------------
// Find next highest floor in surrounding sectors
//----------------------------------------------------------------------------------------------------------------------
Fixed P_FindNextHighestFloor(sector_t& sec, const Fixed currentheight) noexcept {
    Fixed height = 0x7FFFFFFF;  // Init to the maximum Fixed
    const uint32_t numLines = sec.linecount;
    line_t** const ppSectorLines = sec.lines;

    for (uint32_t i = 0; i < numLines; ++i) {
        ASSERT(ppSectorLines[i]);
        line_t& line = *ppSectorLines[i];
        const sector_t* const pOther = getNextSector(line, sec);
        if (pOther) {
            if (pOther->floorheight > currentheight) {      // Higher than current?
                if (pOther->floorheight < height) {         // Lower than result?
                    height = pOther->floorheight;           // Change result
                }
            }
        }
    }

    return height;
}

//----------------------------------------------------------------------------------------------------------------------
// Find lowest ceiling in the surrounding sectors
//----------------------------------------------------------------------------------------------------------------------
Fixed P_FindLowestCeilingSurrounding(sector_t& sec) noexcept {
    Fixed height = 0x7FFFFFFF;  // Heighest ceiling possible
    const uint32_t numLines = sec.linecount;
    line_t** const ppSectorLines = sec.lines;

    for (uint32_t i = 0; i < numLines; ++i) {
        ASSERT(ppSectorLines[i]);
        line_t& line = *ppSectorLines[i];
        const sector_t* const pOther = getNextSector(line, sec);
        if (pOther) {
            if (pOther->ceilingheight < height) {   // Lower?
                height = pOther->ceilingheight;     // Set the new height
            }
        }
    }

    return height;
}

//----------------------------------------------------------------------------------------------------------------------
// Find highest ceiling in the surrounding sectors
//----------------------------------------------------------------------------------------------------------------------
Fixed P_FindHighestCeilingSurrounding(sector_t& sec) noexcept {
    Fixed height = 0x80000000;  // Lowest ceiling possible
    const uint32_t numLines = sec.linecount;
    line_t** const ppSectorLines = sec.lines;

    for (uint32_t i = 0; i < numLines; ++i) {
        ASSERT(ppSectorLines[i]);
        line_t& line = *ppSectorLines[i];
        const sector_t* const pOther = getNextSector(line, sec);
        if (pOther) {
            if (pOther->ceilingheight > height) {    // Higher?
                height = pOther->ceilingheight;      // Save the highest
            }
        }
    }

    return height;  // Return highest
}

//----------------------------------------------------------------------------------------------------------------------
// Return next sector # that line tag refers to
//----------------------------------------------------------------------------------------------------------------------
uint32_t P_FindSectorFromLineTag(line_t& line, const uint32_t start) noexcept {
    const uint32_t tag = line.tag;  // Cache the tag

    for (uint32_t sectorIdx = start + 1; sectorIdx < gNumSectors; ++sectorIdx) {
        sector_t& sec = gpSectors[sectorIdx];   // Get the sector pointer
        if (sec.tag == tag) {                   // Tag match?
            return sectorIdx;                   // Return the index
        }
    }

    return UINT32_MAX;  // No good...
}

//----------------------------------------------------------------------------------------------------------------------
// Find minimum light from an adjacent sector
//----------------------------------------------------------------------------------------------------------------------
uint32_t P_FindMinSurroundingLight(sector_t& sector, uint32_t max) noexcept {
    uint32_t min = max; // Assume answer
    const uint32_t numLines = sector.linecount;
    line_t** const ppSectorLines = sector.lines;

    for (uint32_t i = 0; i < numLines; ++i) {
        ASSERT(ppSectorLines[i]);
        line_t& line = *ppSectorLines[i];
        const sector_t* const pOther = getNextSector(line, sector);
        if (pOther) {
            if (pOther->lightlevel < min) {
                min = pOther->lightlevel;   // Get darker
            }
        }
    }

    return min;
}

//----------------------------------------------------------------------------------------------------------------------
// Called every time a thing origin is about to cross a line with a non 0 special
//----------------------------------------------------------------------------------------------------------------------
void P_CrossSpecialLine(line_t& line, mobj_t& thing) noexcept {
    // Triggers that other things other than players can activate
    if (!thing.player) {
        switch (line.special) {
            default:        // None of the above?
                return;     // Exit
            case 39:        // Teleport trigger
            case 97:        // Teleport retrigger
            case 4:         // Raise door
            case 10:        // Plat down-wait-up-stay trigger
            case 88:        // Plat down-wait-up-stay retrigger
                break;      // Null event
        }
    }
    
    switch (line.special) {
        //--------------------------------------------------------------------------------------------------------------
        // The first group of triggers all clear line->special so that they can't be triggered again.
        // The second group leaves line->special alone so triggering can occur at will.
        //--------------------------------------------------------------------------------------------------------------

        case 2: // Open Door
            EV_DoDoor(line, open);
            line.special = 0;
            break;
        case 3: // Close Door
            EV_DoDoor(line, close);
            line.special = 0;
            break;
        case 4: // Raise Door
            EV_DoDoor(line, normaldoor);
            line.special = 0;
            break;
        case 5: // Raise Floor
            EV_DoFloor(line, raiseFloor);
            line.special = 0;
            break;
        case 6: // Fast Ceiling Crush & Raise
            EV_DoCeiling(line, fastCrushAndRaise);
            line.special = 0;
            break;
        case 8: // Build Stairs
            EV_BuildStairs(line);
            line.special = 0;
            break;
        case 10:    // PlatDownWaitUp
            EV_DoPlat(line, downWaitUpStay, 0);
            line.special = 0;
            break;
        case 12:    // Light Turn On - brightest near
            EV_LightTurnOn(line, 0);
            line.special = 0;
            break;
        case 13:    // Light Turn On 255
            EV_LightTurnOn(line, 255);
            line.special = 0;
            break;
        case 16:    // Close Door 30
            EV_DoDoor(line, close30ThenOpen);
            line.special = 0;
            break;
        case 17:    // Start Light Strobing
            EV_StartLightStrobing(line);
            line.special = 0;
            break;
        case 19:    // Lower Floor
            EV_DoFloor(line, lowerFloor);
            line.special = 0;
            break;
        case 22:    // Raise floor to nearest height and change texture
            EV_DoPlat(line, raiseToNearestAndChange, 0);
            line.special = 0;
            break;
        case 25:    // Ceiling Crush and Raise
            EV_DoCeiling(line, crushAndRaise);
            line.special = 0;
            break;
        case 30:    // Raise floor to shortest texture height on either side of lines
            EV_DoFloor(line, raiseToTexture);
            line.special = 0;
            break;
        case 35:    // Lights Very Dark
            EV_LightTurnOn(line, 35);
            line.special = 0;
            break;
        case 36:    // Lower Floor (TURBO)
            EV_DoFloor(line, turboLower);
            line.special = 0;
            break;
        case 37:    // LowerAndChange
            EV_DoFloor(line, lowerAndChange);
            line.special = 0;
            break;
        case 38:    // Lower Floor To Lowest
            EV_DoFloor(line, lowerFloorToLowest);
            line.special = 0;
            break;
        case 39:    // Teleport!
            EV_Teleport(line, thing);
            line.special = 0;
            break;
        case 40:    // RaiseCeilingLowerFloor
            EV_DoCeiling(line, raiseToHighest);
            EV_DoFloor(line, lowerFloorToLowest);
            line.special = 0;
            break;
        case 44:    // Ceiling Crush
            EV_DoCeiling(line, lowerAndCrush);
            line.special = 0;
            break;
        case 52:    // Exit!
            G_ExitLevel();
            line.special = 0;
            break;
        case 53:    // Perpetual Platform Raise
            EV_DoPlat(line, perpetualRaise, 0);
            line.special = 0;
            break;
        case 54:    // Platform Stop
            EV_StopPlat(line);
            line.special = 0;
            break;
        case 56:    // Raise Floor Crush
            EV_DoFloor(line, raiseFloorCrush);
            line.special = 0;
            break;
        case 57:    // Ceiling Crush Stop
            EV_CeilingCrushStop(line);
            line.special = 0;
            break;
        case 58:    // Raise Floor 24
            EV_DoFloor(line, raiseFloor24);
            line.special = 0;
            break;
        case 59:    // Raise Floor 24 And Change
            EV_DoFloor(line, raiseFloor24AndChange);
            line.special = 0;
            break;
        case 104:   // Turn lights off in sector(tag)
            EV_TurnTagLightsOff(line);
            line.special = 0;
            break;

        //--------------------------------------------------------------------------------------------------------------
        // These are restartable, do not affect line->special
        //--------------------------------------------------------------------------------------------------------------

        case 72:    // Ceiling Crush
            EV_DoCeiling(line, lowerAndCrush);
            break;
        case 73:    // Ceiling Crush and Raise
            EV_DoCeiling(line, crushAndRaise);
            break;
        case 74:    // Ceiling Crush Stop
            EV_CeilingCrushStop(line);
            break;
        case 75:    // Close Door
            EV_DoDoor(line, close);
            break;
        case 76:    // Close Door 30
            EV_DoDoor(line, close30ThenOpen);
            break;
        case 77:    // Fast Ceiling Crush & Raise
            EV_DoCeiling(line, fastCrushAndRaise);
            break;
        case 79:    // Lights Very Dark
            EV_LightTurnOn(line, 35);
            break;
        case 80:    // Light Turn On - brightest near
            EV_LightTurnOn(line, 0);
            break;
        case 81:    // Light Turn On 255
            EV_LightTurnOn(line, 255);
            break;
        case 82:    // Lower Floor To Lowest
            EV_DoFloor(line, lowerFloorToLowest);
            break;
        case 83:    // Lower Floor
            EV_DoFloor(line, lowerFloor);
            break;
        case 84:    // LowerAndChange
            EV_DoFloor(line, lowerAndChange);
            break;
        case 86:    // Open Door
            EV_DoDoor(line, open);
            break;
        case 87:    // Perpetual Platform Raise
            EV_DoPlat(line, perpetualRaise, 0);
            break;
        case 88:    // PlatDownWaitUp
            EV_DoPlat(line, downWaitUpStay, 0);
            break;
        case 89:    // Platform Stop
            EV_StopPlat(line);
            break;
        case 90:    // Raise Door
            EV_DoDoor(line, normaldoor);
            break;
        case 91:    // Raise Floor
            EV_DoFloor(line, raiseFloor);
            break;
        case 92:    // Raise Floor 24
            EV_DoFloor(line, raiseFloor24);
            break;
        case 93:    // Raise Floor 24 And Change
            EV_DoFloor(line, raiseFloor24AndChange);
            break;
        case 94:    // Raise Floor Crush
            EV_DoFloor(line, raiseFloorCrush);
            break;
        case 95:    // Raise floor to nearest height and change texture
            EV_DoPlat(line, raiseToNearestAndChange, 0);
            break;
        case 96:    // Raise floor to shortest texture height on either side of lines
            EV_DoFloor(line, raiseToTexture);
            break;
        case 97:    // Teleport!
            EV_Teleport(line, thing);
            break;
        case 98:    // Lower Floor (TURBO)
            EV_DoFloor(line, turboLower);
            break;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Called when a thing shoots a special line.
//----------------------------------------------------------------------------------------------------------------------
void P_ShootSpecialLine(mobj_t& thing, line_t& line) noexcept {
    // Impacts that other things can activate
    if (!thing.player) {
        if (line.special != 46) {   // Open door impact
            return;
        }
    }

    switch (line.special) {
        case 24:    // Raise floor
            EV_DoFloor(line, raiseFloor);
            P_ChangeSwitchTexture(line, false);
            break;
        case 46:    // Open door
            EV_DoDoor(line, open);
            P_ChangeSwitchTexture(line, true);
            break;
        case 47:    // Raise floor near and change
            EV_DoPlat(line, raiseToNearestAndChange,0);
            P_ChangeSwitchTexture(line, false);
            break;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Called every tic frame that the player origin is in a special sector.
// Used for radioactive slime.
//----------------------------------------------------------------------------------------------------------------------
void PlayerInSpecialSector(player_t& player, sector_t& sector) noexcept {
    ASSERT(player.mo);

    if (player.mo->z != sector.floorheight) {
        return;     // Not all the way down yet
    }

    uint32_t damage = 0;    // No damage taken initally
    switch (sector.special) {
        case 5: // Hellslime damage
            damage = 10;
            break;
        case 7:     // Nukage damage
            damage = 5;
            break;
        case 16:    // Super hellslime damage
        case 4:     // Strobe hurt
            damage = 20;
            if (Random::nextU32(255) < 5) {     // Chance it didn't hurt
                damage |= 0x8000;               // Suit didn't help!
            }
            break;
        case 9:     // Found a secret sector
            ++player.secretcount;
            sector.special = 0;        // Remove the special
    }

    if ((damage != 0) && gbTick1) {     // Time for pain?
        if (((damage & 0x8000) != 0 ) || (!player.powers[pw_ironfeet])) {   // Inflict?
            DamageMObj(*player.mo, nullptr, nullptr, damage & 0x7FFF);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Animate planes, scroll walls, etc
//----------------------------------------------------------------------------------------------------------------------
void P_UpdateSpecials() noexcept {
    // Animate flats and textures globaly periodically
    if (gbTick4) {
        uint32_t i = 0;
        anim_t* AnimPtr = gFlatAnims;   // Index to the flat anims

        do {
            ++AnimPtr->CurrentPic;                                  // Next picture index
            if (AnimPtr->CurrentPic >= AnimPtr->LastPicNum+1) {     // Off the end?
                AnimPtr->CurrentPic = AnimPtr->BasePic;             // Reset the animation
            }

            // Set the frame
            Textures::setFlatAnimTexNum(AnimPtr->LastPicNum, AnimPtr->CurrentPic);
            ++AnimPtr;
        } while (++i < gNumFlatAnims);
    }

    // Animate line specials
    {
        line_t** ppCurLine = gppLineSpecialList;
        line_t** const ppEndLine = gppLineSpecialList + gNumLineSpecials;

        while (ppCurLine < ppEndLine) {
            line_t& theline = *ppCurLine[0];

            if (theline.special == 48) {
                // Effect firstcol scroll: scroll the texture
                side_t& side = *theline.SidePtr[0];
                side.texXOffset += 1.0f;
                side.texXOffset = std::fmod(side.texXOffset, 256.0f);   // Wrap to prevent precision getting out of control
            }

            ++ppCurLine;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// After the map has been loaded, scan for specials that spawn thinkers
//----------------------------------------------------------------------------------------------------------------------
void SpawnSpecials() noexcept {
    // Init special SECTORs
    PurgeLineSpecials();    // Make SURE they are gone

    {
        const uint32_t numSectors = gNumSectors;
        sector_t* const pSectors = gpSectors;

        for (uint32_t i = 0; i < numSectors; ++i) {
            sector_t& sector = pSectors[i];

            switch (sector.special) {
                case 1:     // FLICKERING LIGHTS
                    P_SpawnLightFlash(sector);
                    break;
                case 2:     // STROBE FAST
                    P_SpawnStrobeFlash(sector, FASTDARK, false);
                    break;
                case 3:     // STROBE SLOW
                    P_SpawnStrobeFlash(sector, SLOWDARK, false);
                    break;
                case 8:     // GLOWING LIGHT
                    P_SpawnGlowingLight(sector);
                    break;
                case 9:     // SECRET SECTOR
                    ++gSecretsFoundInLevel;
                    break;
                case 10:    // DOOR CLOSE IN 30 SECONDS
                    P_SpawnDoorCloseIn30(sector);
                    break;
                case 12:    // SYNC STROBE SLOW
                    P_SpawnStrobeFlash(sector, SLOWDARK, true);
                    break;
                case 13:    // SYNC STROBE FAST
                    P_SpawnStrobeFlash(sector, FASTDARK, true);
                    break;
                case 14:    // DOOR RAISE IN 5 MINUTES
                    P_SpawnDoorRaiseIn5Mins(sector);
            }
        }
    }

    // Init line EFFECTs, first pass, count the effects detected
    const uint32_t numLines = gNumLines;
    line_t* const pLines = gpLines;

    {
        gNumLineSpecials = 0;   // No specials found initially

        // Traverse the list
        for (uint32_t i = 0; i < numLines; ++i) {
            line_t& line = pLines[i];
            if (line.special == 48) {   // Effect firstcol scroll+
                ++gNumLineSpecials;
            }
        }
    }

    // Any found?
    if (gNumLineSpecials > 0) {
        // Get memory for the list
        line_t** ppLinelist = (line_t**) MemAlloc(sizeof(line_t*) * gNumLineSpecials);
        gppLineSpecialList = ppLinelist;    // Save the pointer
        
        for (uint32_t i = 0; i < numLines; ++i) {
            line_t& line = pLines[i];
            if (line.special == 48) {   // Effect firstcol scroll+
                ppLinelist[0] = &line;
                ++ppLinelist;
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Release the memory for line specials
//----------------------------------------------------------------------------------------------------------------------
void PurgeLineSpecials() noexcept {
    if (gppLineSpecialList) {                       // Is there a valid pointer?
        MEM_FREE_AND_NULL(gppLineSpecialList);      // Release it
        gNumLineSpecials = 0;                       // No lines
    }
}
