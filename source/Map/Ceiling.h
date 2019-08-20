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

bool EV_DoCeiling(line_t& line, const ceiling_e type) noexcept;
bool EV_CeilingCrushStop(line_t& line) noexcept;
void ResetCeilings() noexcept;
