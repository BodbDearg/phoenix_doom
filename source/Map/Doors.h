#pragma once

struct line_t;
struct mobj_t;
struct sector_t;

// Enums for door types
enum vldoor_e {
    normaldoor,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins
};

bool EV_DoDoor(line_t& line, const vldoor_e type) noexcept;
void EV_VerticalDoor(line_t& line, mobj_t& thing) noexcept;
void P_SpawnDoorCloseIn30(sector_t& sec) noexcept;
void P_SpawnDoorRaiseIn5Mins(sector_t& sec) noexcept;
