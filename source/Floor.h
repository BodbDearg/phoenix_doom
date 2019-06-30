#pragma once

#include "Doom.h"

result_e T_MovePlane(
    sector_t*sector,
    Fixed speed,
    Fixed dest,
    bool crush,
    bool Ceiling,
    int direction
);

bool EV_DoFloor(line_t* line, floor_e floortype);
bool EV_BuildStairs(line_t* line);
bool EV_DoDonut(line_t* line);
