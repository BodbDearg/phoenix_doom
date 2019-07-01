#pragma once

struct line_t;

// Enums for ceiling types
enum ceiling_e {          
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise
};

bool EV_DoCeiling(line_t* line, ceiling_e type);
bool EV_CeilingCrushStop(line_t* line);
void ResetCeilings();
