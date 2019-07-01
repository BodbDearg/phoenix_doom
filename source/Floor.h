#pragma once

#include "Doom.h"

struct line_t;
struct sector_t;

// Enums for moving sector results
enum result_e {
    moveok,
    crushed,
    pastdest
};

// Enums for floor types
enum floor_e {      
    lowerFloor,                 // lower floor to highest surrounding floor
    lowerFloorToLowest,         // lower floor to lowest surrounding floor
    turboLower,                 // lower floor to highest surrounding floor VERY FAST
    raiseFloor,                 // raise floor to lowest surrounding CEILING
    raiseFloorToNearest,        // raise floor to next highest surrounding floor
    raiseToTexture,             // raise floor to shortest height texture around it
    lowerAndChange,             // lower floor to lowest surrounding floor and change floorpic
    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,
    donutRaise
};

result_e T_MovePlane(
    sector_t*sector,
    Fixed speed,
    Fixed dest,
    bool crush,
    bool Ceiling,
    int32_t direction
);

bool EV_DoFloor(line_t* line, floor_e floortype);
bool EV_BuildStairs(line_t* line);
bool EV_DoDonut(line_t* line);
