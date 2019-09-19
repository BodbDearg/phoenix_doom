#include "Doors.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Floor.h"
#include "Game/Tick.h"
#include "MapData.h"
#include "Specials.h"
#include "Things/MapObj.h"
#include "UI/StatusBarUI.h"

static constexpr Fixed      VDOORSPEED  = 6 << FRACBITS;                // Speed to open a vertical door
static constexpr uint32_t   VDOORWAIT   = ((TICKSPERSEC * 14) / 3);     // Door time to wait before closing 4.6 seconds

struct vldoor_t {
    sector_t*   sector;         // Sector being modified
    Fixed       topheight;      // Topmost height
    Fixed       speed;          // Speed of door motion
    int         direction;      // 1 = up, 0 = waiting at top, -1 = down
    uint32_t    topwait;        // Tics to wait at the top (keep in case a door going down is reset)
    uint32_t    topcountdown;   // when it reaches 0, start going down
    vldoor_e    type;           // Type of door
};

//----------------------------------------------------------------------------------------------------------------------
// Think logic for doors
//----------------------------------------------------------------------------------------------------------------------
static void T_VerticalDoor(vldoor_t& door) noexcept {
    ASSERT(door.sector);

    switch (door.direction) {
        case 0: {   // Waiting or in stasis
            if (door.topcountdown > 1) {
                --door.topcountdown;
            } else {
                door.topcountdown = 0;          // Force zero
                switch (door.type) {
                    case normaldoor:
                        door.direction = -1;    // Time to go back down
                        S_StartSound(&door.sector->SoundX, sfx_dorcls);
                        break;

                    case close30ThenOpen:
                        door.direction = 1;
                        S_StartSound(&door.sector->SoundX, sfx_doropn);
                        break;

                    // DC: these cases were unhandled
                    case close:
                    case open:
                    case raiseIn5Mins:
                        break;
                }
            }
        }   break;

        case 2: {   // Initial wait
            if (door.topcountdown > 1) {
                --door.topcountdown;
            } else {
                door.topcountdown = 0;  // Force zero
                if (door.type == raiseIn5Mins) {
                    door.direction = 1;
                    door.type = normaldoor;
                    S_StartSound(&door.sector->SoundX, sfx_doropn);
                }
            }
        } break;

        case -1: {  // Going down
            const result_e res = T_MovePlane(
                *door.sector,
                door.speed,
                door.sector->floorheight,
                false,
                true,
                door.direction
            );

            if (res == pastdest) {      // Finished closing?
                switch (door.type) {
                    case normaldoor:
                    case close:
                        door.sector->specialdata = nullptr;         // Remove it
                        RemoveThinker(door);                        // unlink and free
                        break;

                    case close30ThenOpen:
                        door.direction = 0;                         // Waiting
                        door.topcountdown = TICKSPERSEC * 30;
                        break;

                    case open:                                      // DC: these cases were unhandled
                    case raiseIn5Mins:
                        break;
                }
            } else if (res == crushed) {
                door.direction = 1; // Move back up
                S_StartSound(&door.sector->SoundX, sfx_doropn);
            }
        }   break;

        case 1: {   // Going up
            const result_e res = T_MovePlane(
                *door.sector,
                door.speed,
                door.topheight,
                false,
                true,
                door.direction
            );

            if (res == pastdest) {      // Fully opened?
                switch (door.type) {
                    case normaldoor:
                        door.direction = 0;                     // Wait at top
                        door.topcountdown = door.topwait;       // Reset timer
                        break;

                    case close30ThenOpen:
                    case open:
                        door.sector->specialdata = nullptr;
                        RemoveThinker(door);                    // Unlink and free
                        break;

                    case close:                                 // DC: these cases were unhandled
                    case raiseIn5Mins:
                        break;
                }
            }
        }   break;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Move a door up/down and all around!
//----------------------------------------------------------------------------------------------------------------------
bool EV_DoDoor(line_t& line, const vldoor_e type) noexcept {
    uint32_t secnum = UINT32_MAX;
    bool rtn = false;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) != UINT32_MAX) {
        sector_t& sec = gpSectors[secnum];
        if (sec.specialdata) {  // Already something here?
            continue;
        }

        // New door thinker
        rtn = true;

        vldoor_t& door = AddThinker(T_VerticalDoor);
        sec.specialdata = &door;
        door.sector = &sec;
        door.type = type;               // Save the type
        door.speed = VDOORSPEED;        // Save the speed
        door.topwait = VDOORWAIT;       // Save the initial delay

        switch (type) {
            case close:
                door.topheight = P_FindLowestCeilingSurrounding(sec);
                door.topheight -= 4 * FRACUNIT;
                door.direction = -1;    // Down
                S_StartSound(&door.sector->SoundX, sfx_dorcls);
                break;
            case close30ThenOpen:
                door.topheight = sec.ceilingheight;
                door.direction = -1;    // Down
                S_StartSound(&door.sector->SoundX, sfx_dorcls);
                break;
            case normaldoor:
            case open:
                door.direction = 1;     // Up
                door.topheight = P_FindLowestCeilingSurrounding(sec);
                door.topheight -= 4 * FRACUNIT;
                if (door.topheight != sec.ceilingheight) {
                    S_StartSound(&door.sector->SoundX, sfx_doropn);
                }
                break;
            case raiseIn5Mins:  // DC: unhandled cases
                break;
        }
    }
    return rtn;
}

//----------------------------------------------------------------------------------------------------------------------
// Open a door manually, no tag value
//----------------------------------------------------------------------------------------------------------------------
void EV_VerticalDoor(line_t& line, mobj_t& thing) noexcept {
    ASSERT(line.backsector);

    // Is this a player? Only player's have trouble with locks...
    if (thing.player) {
        player_t& player = *thing.player;
        uint32_t i = UINT32_MAX;    // Don't quit!

        switch (line.special) {
            case 26:    // Blue Card Lock
            case 32:
            case 99:    // Blue Skull Lock
            case 106:
                if ((!player.cards[it_bluecard]) && (!player.cards[it_blueskull])) {
                    i = (line.special < 99) ? it_bluecard : it_blueskull;
                }
                break;

            case 27:    // Yellow Card Lock
            case 34:
            case 105:   // Yellow Skull Lock
            case 108:
                if ((!player.cards[it_yellowcard]) && (!player.cards[it_yellowskull])) {
                    i = (line.special < 105) ? it_yellowcard : it_yellowskull;
                }
                break;

            case 28:    // Red Card Lock
            case 33:
            case 100:   // Red Skull Lock
            case 107:
                if ((!player.cards[it_redcard]) && (!player.cards[it_redskull])) {
                    i = (line.special < 100) ? it_redcard : it_redskull;
                }
                break;
        }

        if (i != UINT32_MAX) {
            S_StartSound(&thing.x, sfx_oof);    // Play the sound
            gStBar.tryopen[i] = true;           // Trigger on status bar
            return;
        }
    }

    // If the sector has an active thinker, use it
    sector_t& sec = *line.backsector;

    if (sec.specialdata) {
        vldoor_t& door = *(vldoor_t*) sec.specialdata;  // Use existing

        switch (line.special) {
            case   1:   // Only for "raise" doors, not "open"s
            case  26:   // Blue card
            case  27:   // Yellow card
            case  28:   // Red card
            case 106:   // Blue skull
            case 108:   // Yellow skull
            case 107:   // Red skull
                if (door.direction == -1) {     // Closing?
                    door.direction = 1;         // Go back up
                } else if (thing.player) {      // Only players make it close
                    door.direction = -1;        // Start going down immediately
                }
                return;     // Exit now
        }
    }

    // For proper sound
    switch (line.special) {
        case 1:     // Normal door sound
        case 31:
            S_StartSound(&sec.SoundX, sfx_doropn);
            break;
        default:    // Locked door sound
            S_StartSound(&sec.SoundX, sfx_doropn);
            break;
    }

    // New door thinker
    vldoor_t& door = AddThinker(T_VerticalDoor);
    sec.specialdata = &door;
    door.sector = &sec;
    door.direction = 1;             // Going up!
    door.speed = VDOORSPEED;        // Set the speed
    door.topwait = VDOORWAIT;

    switch (line.special) {
        case 1:
        case 26:
        case 27:
        case 28:
            door.type = normaldoor;     // Normal open/close door
            break;
        case 31:
        case 32:
        case 33:
        case 34:
            door.type = open;   // Open forever
            break;
    }

    // Find the top and bottom of the movement range
    door.topheight = P_FindLowestCeilingSurrounding(sec) - (4 << FRACBITS);
}

//----------------------------------------------------------------------------------------------------------------------
// Spawn a door that closes after 30 seconds
//----------------------------------------------------------------------------------------------------------------------
void P_SpawnDoorCloseIn30(sector_t& sec) noexcept {
    vldoor_t& door = AddThinker(T_VerticalDoor);
    sec.specialdata = &door;
    sec.special = 0;
    door.sector = &sec;
    door.direction = 0;        // Standard wait
    door.type = normaldoor;
    door.speed = VDOORSPEED;
    door.topcountdown = 30 * TICKSPERSEC;
}

//----------------------------------------------------------------------------------------------------------------------
// Spawn a door that opens after 5 minutes
//----------------------------------------------------------------------------------------------------------------------
void P_SpawnDoorRaiseIn5Mins(sector_t& sec) noexcept {
    vldoor_t& door = AddThinker(T_VerticalDoor);
    sec.specialdata = &door;
    sec.special = 0;
    door.sector = &sec;
    door.direction = 2; // Initial wait
    door.type = raiseIn5Mins;
    door.speed = VDOORSPEED;
    door.topheight = P_FindLowestCeilingSurrounding(sec) - (4 << FRACBITS);
    door.topwait = VDOORWAIT;
    door.topcountdown = 5 * 60 * TICKSPERSEC;
}
