#include "Platforms.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Random.h"
#include "Floor.h"
#include "Game/Data.h"
#include "Game/Tick.h"
#include "MapData.h"
#include "Specials.h"

static constexpr Fixed      PLATSPEED   = 2 << FRACBITS;        // Speed of platform motion
static constexpr uint32_t   PLATWAIT    = 3 * TICKSPERSEC;      // Delay in ticks before platform motion

// Current action for the platform
enum plat_e {
    up,
    down,
    waiting,
    in_stasis
};

// Structure for a moving platform
struct plat_t {
    sector_t*       sector;     // Sector the platform will modify
    plat_t*         next;       // Next entry in the linked list
    Fixed           speed;      // Speed of motion
    Fixed           low;        // Lowest Y point
    Fixed           high;       // Highest Y point
    uint32_t        wait;       // Wait in ticks before moving back (Zero = never)
    uint32_t        count;      // Running count (If zero then wait forever!)
    uint32_t        tag;        // Event ID tag
    plat_e          status;     // Current status (up,down,wait,dead)
    plat_e          oldstatus;  // Previous status
    plattype_e      type;       // Type of platform
    bool            crush;      // Can it crush things?
};

static plat_t* gpMainPlatPtr;   // Pointer to the first plat in list

//----------------------------------------------------------------------------------------------------------------------
// Insert a new platform record into the linked list, use a simple insert into the head to add.
//----------------------------------------------------------------------------------------------------------------------
static void AddActivePlat(plat_t& plat) noexcept {
    plat.next = gpMainPlatPtr;      // Insert into the chain
    gpMainPlatPtr = &plat;          // Set this one as the first
}

//----------------------------------------------------------------------------------------------------------------------
// Remove a platform record from the linked list.
// I also call RemoveThinker to actually perform the memory disposal.
//----------------------------------------------------------------------------------------------------------------------
static void RemoveActivePlat(plat_t& platToKill) noexcept {
    plat_t* pPrevPlat = nullptr;        // Init the previous pointer
    plat_t* pPlat = gpMainPlatPtr;      // Get the main list entry

    while (pPlat) {                         // Failsafe!
        if (pPlat == &platToKill) {         // Master pointer matches?
            pPlat = pPlat->next;            // Get the next link
            if (!pPrevPlat) {               // First one in chain?
                gpMainPlatPtr = pPlat;      // Make the next one the new head
            } else {
                pPrevPlat->next = pPlat;    // Remove the link
            }
            break;                          // Get out of the loop
        }
        pPrevPlat = pPlat;                  // Make the current pointer the previous one
        pPlat = pPlat->next;                // Get the next link
    }

    platToKill.sector->specialdata = nullptr;   // Unlink from the sector
    RemoveThinker(platToKill);                  // Remove the thinker
}

//----------------------------------------------------------------------------------------------------------------------
// Move a plat up and down; there are only 4 states a platform can be in...
// Going up, Going down, waiting to either go up or down or finally not active and
// needing an external event to trigger it.
//----------------------------------------------------------------------------------------------------------------------
static void T_PlatRaise(plat_t& plat) noexcept {
    // Which state is the platform in?
    switch (plat.status) {
        // Going up?
        case up: {
            const result_e res = T_MovePlane(*plat.sector, plat.speed, plat.high, plat.crush, false, 1);
            if ((plat.type == raiseAndChange) || (plat.type == raiseToNearestAndChange)) {
                if (gbTick2) {  // Make the rumbling sound
                    S_StartSound(&plat.sector->SoundX,sfx_stnmov);
                }
            }

            if ((res == crushed) && (!plat.crush)) {                // Crushed something?
                plat.count = plat.wait;                             // Get the delay time
                plat.status = down;                                 // Going the opposite direction                
                S_StartSound(&plat.sector->SoundX, sfx_pstart);
            } else if (res == pastdest) {                           // Moved too far?
                plat.count = plat.wait;                             // Reset the timer
                plat.status = waiting;                              // Make it wait
                S_StartSound(&plat.sector->SoundX, sfx_pstop);

                switch (plat.type) {                // What type of platform is it?
                    case downWaitUpStay:            // Shall it stay here forever?
                    case raiseAndChange:            // Change the texture and exit?
                        RemoveActivePlat(plat);     // Remove it then
                        break;

                    case perpetualRaise:            // DC: Nothing to be done for these cases
                    case raiseToNearestAndChange:
                        break;
                }
            }
        }   break;

        // Going down?
        case down: {
            const result_e res = T_MovePlane(*plat.sector, plat.speed, plat.low, false, false, -1);
            if (res == pastdest) {          // Moved too far
                plat.count = plat.wait;     // Set the delay count
                plat.status = waiting;      // Delay mode
                S_StartSound(&plat.sector->SoundX, sfx_pstop);
            }
        }   break;

        case waiting: {
            if (plat.count) {               // If waiting will expire...
                if (plat.count > 1) {       // Time up?
                    --plat.count;           // Remove the time (But leave 1)
                } else {
                    if (plat.sector->floorheight == plat.low) {     // At the bottom?
                        plat.status = up;                           // Move up
                    } else {
                        plat.status = down;                         // Move down
                    }
                    S_StartSound(&plat.sector->SoundX, sfx_pstart);
                }
            }
        }   break;

        case in_stasis:     // DC: Nothing to be done for this case
            break;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Given a tag ID number, activate a platform that is currently inactive.
//----------------------------------------------------------------------------------------------------------------------
static void ActivateInStasis(const uint32_t tag) noexcept {
    plat_t* pPlat = gpMainPlatPtr;                                  // Get the main list entry
    while (pPlat) {                                                 // Scan all entries in the thinker list
        if (pPlat->tag == tag && pPlat->status == in_stasis) {      // Match?
            pPlat->status = pPlat->oldstatus;                       // Restart the platform
            ChangeThinkCode(*pPlat, T_PlatRaise);                   // Reset code
        }
        pPlat = pPlat->next;    // Get the next link
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Trigger a platform. Perform the action for all platforms "amount" is only used for SOME platforms.
//----------------------------------------------------------------------------------------------------------------------
bool EV_DoPlat(line_t& line, plattype_e type, uint32_t amount) noexcept {
    // Assume false initially
    bool rtn = false; 

    // Activate all <type> plats that are in_stasis
    if (type == perpetualRaise) {
        ActivateInStasis(line.tag);     // Reset the platforms
    }

    uint32_t secnum = UINT32_MAX;       // Which sector am I in?

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) != UINT32_MAX) {
        sector_t& sec = gpSectors[secnum];      // Get the sector pointer
        if (sec.specialdata) {                  // Already has a platform?
            continue;                           // Skip
        }

        // Find lowest & highest floors around sector
        rtn = true;                                 // I created a sector
        plat_t& plat = AddThinker(T_PlatRaise);     // Add to the thinker list
        plat.type = type;                           // Save the platform type
        plat.sector = &sec;                         // Save the sector pointer
        plat.sector->specialdata = &plat;           // Point back to me...
        plat.crush = false;                         // Can't crush anything
        plat.tag = line.tag;                        // Assume the line's ID

        // Init vars based on type
        switch (type) {
            case raiseToNearestAndChange: {     // Go up and stop
                sec.special = 0;                // If lava, then stop hurting the player
                plat.high = P_FindNextHighestFloor(sec, sec.floorheight);
                goto RaisePart2;
            }

            case raiseAndChange: {  // Raise a specific amount and stop
                plat.high = sec.floorheight + (amount << FRACBITS);
            RaisePart2:
                plat.speed = PLATSPEED / 2;                     // Slow speed
                sec.FloorPic = line.frontsector->FloorPic;
                plat.wait = 0;                                  // No delay before moving
                plat.status = up;                               // Going up!
                S_StartSound(&sec.SoundX, sfx_stnmov);          // Begin move
            }   break;

            case downWaitUpStay: {
                plat.speed = PLATSPEED * 4;                     // Fast speed
                plat.low = P_FindLowestFloorSurrounding(sec);   // Lowest floor
                if (plat.low > sec.floorheight) {
                    plat.low = sec.floorheight;                 // Go to the lowest mark
                }
                plat.high = sec.floorheight;                    // Allow to return
                plat.wait = PLATWAIT;                           // Set the delay when it hits bottom
                plat.status = down;                             // Go down
                S_StartSound(&sec.SoundX, sfx_pstart);
            }   break;

            case perpetualRaise: {
                plat.speed = PLATSPEED; // Set normal speed
                plat.low = P_FindLowestFloorSurrounding(sec);
                if (plat.low > sec.floorheight) {
                    plat.low = sec.floorheight;     // Set lower mark
                }
                plat.high = P_FindHighestFloorSurrounding(sec);
                if (plat.high < sec.floorheight) {
                    plat.high = sec.floorheight;    // Set highest mark
                }
                plat.wait = PLATWAIT;                       // Delay when it hits bottom
                plat.status = (plat_e) Random::nextU8(1);   // Up or down
                S_StartSound(&sec.SoundX, sfx_pstart);      // Start
            }   break;
        }
        AddActivePlat(plat);    // Add the platform to the list
    }
    return rtn;     // Did I create one?
}

//----------------------------------------------------------------------------------------------------------------------
// Deactivate a platform; only affect platforms that have the same ID tag
// as the line segment and also are active.
//----------------------------------------------------------------------------------------------------------------------
void EV_StopPlat(line_t& line) noexcept {
    const uint32_t tag = line.tag;                                      // Cache the id tag
    plat_t* pPlat = gpMainPlatPtr;                                      // Get the main list entry
    while (pPlat) {                                                     // Scan all entries in the thinker list
        if ((pPlat->tag == tag) && (pPlat->status != in_stasis)) {      // Match?
            pPlat->oldstatus = pPlat->status;                           // Save the platform's state
            pPlat->status = in_stasis;                                  // Now in stasis
            ChangeThinkCode(pPlat, nullptr);                            // Shut down
        }
        pPlat = pPlat->next;                                            // Get the next link
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Reset the master plat pointer; called from InitThinkers
//----------------------------------------------------------------------------------------------------------------------
void ResetPlats() noexcept {
    // FIXME: DC: Investigate if this is a leak
    gpMainPlatPtr = nullptr;    // Forget about the linked list
}
