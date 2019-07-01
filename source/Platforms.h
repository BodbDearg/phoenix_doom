#pragma once

#include <cstdint>

struct line_t;

// Enums for platform types
enum plattype_e {
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange
};

bool EV_DoPlat(line_t* line, plattype_e type, uint32_t amount);
void EV_StopPlat(line_t* line);
void ResetPlats();
