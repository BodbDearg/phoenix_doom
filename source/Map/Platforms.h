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

bool EV_DoPlat(line_t& line, const plattype_e type, const uint32_t amount) noexcept;
void EV_StopPlat(line_t& line) noexcept;
void ResetPlats() noexcept;
