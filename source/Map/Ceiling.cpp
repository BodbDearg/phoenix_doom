#include "Ceiling.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Macros.h"
#include "Floor.h"
#include "Game/Tick.h"
#include "MapData.h"
#include "Specials.h"

static constexpr Fixed CEILSPEED = 2 << FRACBITS;   // Speed of crushing ceilings

struct ceiling_t {
    sector_t*   sector;         // Pointer to the sector structure
    ceiling_t*  next;           // Next entry in the linked list
    Fixed       bottomheight;   // Lowest point to move to
    Fixed       topheight;      // Highest point to move to
    Fixed       speed;          // Speed of motion
    uint32_t    tag;            // ID
    int         direction;      // 1 = up, 0 = waiting, -1 = down
    int         olddirection;   // Previous direction to restart with
    ceiling_e   type;           // Type of ceiling
    bool        crush;          // Can it crush?
};

static ceiling_t* gpMainCeilingPtr;     // Pointer to the first ceiling in list

//------------------------------------------------------------------------------------------------------------------------------------------
// Insert a new ceiling record into the linked list, use a simple insert into the head to add.
//------------------------------------------------------------------------------------------------------------------------------------------
static void AddActiveCeiling(ceiling_t& ceiling) noexcept {
    ceiling.next = gpMainCeilingPtr;    // Insert into the chain
    gpMainCeilingPtr = &ceiling;        // Set this one as the first
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Remove a ceiling record from the linked list.
// I also call RemoveThinker to actually perform the memory disposal.
//------------------------------------------------------------------------------------------------------------------------------------------
static void RemoveActiveCeiling(ceiling_t& killCeiling) noexcept {
    ceiling_t* pPrev = nullptr;                     // Init the previous pointer
    ceiling_t* pCeiling = gpMainCeilingPtr;         // Get the main list entry

    while (pCeiling) {                              // Failsafe!
        if (pCeiling == &killCeiling) {             // Master pointer matches?
            pCeiling = pCeiling->next;              // Get the next link
            if (!pPrev) {                           // First one in chain?
                gpMainCeilingPtr = pCeiling;        // Make the next one the new head
            } else {
                pPrev->next = pCeiling;             // Remove the link
            }
            break;                                  // Get out of the loop
        }
        pPrev = pCeiling;                           // Make the current pointer the previous one
        pCeiling = pCeiling->next;                  // Get the next link
    }

    killCeiling.sector->specialdata = nullptr;      // Unlink from the sector
    RemoveThinker(&killCeiling);                    // Remove the thinker
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Think logic; this code will move the ceiling up and down.
//------------------------------------------------------------------------------------------------------------------------------------------
static void T_MoveCeiling(ceiling_t& ceiling) noexcept {
    ASSERT(ceiling.sector);

    if (ceiling.direction == 1) {   // Going up?
        // Move it
        const result_e res = T_MovePlane(
            *ceiling.sector,
            ceiling.speed,
            ceiling.topheight,
            false,
            true,
            ceiling.direction
        );

        if (gbTick2) {   // Sound?
            S_StartSound(&ceiling.sector->SoundX, sfx_stnmov);
        }

        if (res == pastdest) {  // Did it reach the top?
            switch (ceiling.type) {
                case raiseToHighest:
                    RemoveActiveCeiling(ceiling);   // Remove the thinker
                    break;

                case fastCrushAndRaise:
                case crushAndRaise:
                    ceiling.direction = -1;         // Go down now
                    break;

                case lowerToFloor:                  // DC: nothing to be done for these cases
                case lowerAndCrush:                
                    break;
            }
        }
    } else if (ceiling.direction == -1) {   // Going down?
        // Move it
        const result_e res = T_MovePlane(
            *ceiling.sector,
            ceiling.speed,
            ceiling.bottomheight,
            ceiling.crush,
            true,
            ceiling.direction
        );

        if (gbTick2) {  // Time for sound?
            S_StartSound(&ceiling.sector->SoundX, sfx_stnmov);
        }

        if (res == pastdest) {  // Reached the bottom?
            switch (ceiling.type) {
                case crushAndRaise:
                    ceiling.speed = CEILSPEED;      // Reset the speed ALWAYS
                    [[fallthrough]];
                case fastCrushAndRaise:
                    ceiling.direction = 1;          // Go up now
                    break;

                case lowerAndCrush:
                case lowerToFloor:
                    RemoveActiveCeiling(ceiling);   // Remove it
                    break;

                case raiseToHighest:                // DC: nothing to be done for these cases
                    break;
            }
        } else if (res == crushed) {    // Is it crushing something?
            switch (ceiling.type) {
                case crushAndRaise:
                case lowerAndCrush:
                    ceiling.speed = (CEILSPEED / 8);    // Slow down for more fun!
                    break;

                case lowerToFloor:                      // DC: nothing to be done for these cases
                case raiseToHighest:
                case fastCrushAndRaise:
                    break;
            }
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Given a tag ID number, activate a ceiling that is currently inactive.
//------------------------------------------------------------------------------------------------------------------------------------------
static void ActivateInStasisCeiling(const uint32_t tag) noexcept {
    ceiling_t* pCeiling = gpMainCeilingPtr;
    while (pCeiling) {                                              // Scan all entries in the thinker list
        if ((pCeiling->tag == tag) && (!pCeiling->direction)) {     // Match?
            pCeiling->direction = pCeiling->olddirection;           // Restart the platform
            ChangeThinkCode(*pCeiling, T_MoveCeiling);              // Reset code
        }
        pCeiling = pCeiling->next;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Move a ceiling up/down and all around!
//------------------------------------------------------------------------------------------------------------------------------------------
bool EV_DoCeiling(line_t& line, const ceiling_e type) noexcept {
    // Reactivate in-stasis ceilings...for certain types.
    switch (type) {
        case fastCrushAndRaise:
        case crushAndRaise:
            ActivateInStasisCeiling(line.tag);
            break;

        case lowerToFloor:
        case raiseToHighest:
        case lowerAndCrush:
            break;
    }
    
    uint32_t secnum = UINT32_MAX;
    bool rtn = false;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) != UINT32_MAX) {
        sector_t& sec = gpSectors[secnum];      // Get the sector pointer
        if (sec.specialdata) {                  // Already something is here?
            continue;
        }

        // New ceiling thinker
        rtn = true;
        
        ceiling_t& ceiling = AddThinker(T_MoveCeiling);
        sec.specialdata = &ceiling;     // Pass the pointer
        ceiling.sector = &sec;          // Save the sector ptr
        ceiling.tag = sec.tag;          // Set the tag number
        ceiling.crush = false;          // Assume it can't crush
        ceiling.type = type;            // Set the ceiling type

        switch (type) {
            case fastCrushAndRaise:
                ceiling.crush = true;
                ceiling.topheight = sec.ceilingheight;
                ceiling.bottomheight = sec.floorheight;
                ceiling.direction = -1;             // Down
                ceiling.speed = CEILSPEED * 2;      // Go down fast!
                break;

            case crushAndRaise:
                ceiling.crush = true;
                ceiling.topheight = sec.ceilingheight;      // Floor and ceiling
                [[fallthrough]];
            case lowerAndCrush:
            case lowerToFloor:
                ceiling.bottomheight = sec.floorheight;     // To the floor!
                ceiling.direction = -1;     // Down
                ceiling.speed = CEILSPEED;
                break;

            case raiseToHighest:
                ceiling.topheight = P_FindHighestCeilingSurrounding(sec);
                ceiling.direction = 1;      // Go up
                ceiling.speed = CEILSPEED;
                break;
        }
        AddActiveCeiling(ceiling);      // Add the ceiling to the list
    }
    return rtn;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Deactivate a ceiling. Only affect ceilings that have the same ID tag as the line segment and also are active.
//------------------------------------------------------------------------------------------------------------------------------------------
bool EV_CeilingCrushStop(line_t& line) noexcept {
    bool rtn = false;
    const uint32_t tag = line.tag;                              // Get the tag to look for
    ceiling_t* pCeiling = gpMainCeilingPtr;                     // Get the main list entry
    while (pCeiling) {                                          // Scan all entries in the thinker list
        if ((pCeiling->tag == tag) && pCeiling->direction) {    // Match?
            pCeiling->olddirection = pCeiling->direction;       // Save the platform's state
            ChangeThinkCode(pCeiling, nullptr);                 // Shut down
            pCeiling->direction = 0;                            // In statis
            rtn = true;
        }
        pCeiling = pCeiling->next;  // Get the next link
    }
    return rtn;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Reset the master ceiling pointer; called from InitThinkers
//------------------------------------------------------------------------------------------------------------------------------------------
void ResetCeilings() noexcept {
    // Discard the links.
    // Note: this is NOT a leak - the memory is cleaned up when cleaning up thinkers.
    gpMainCeilingPtr = nullptr;
}
