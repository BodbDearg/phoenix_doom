#pragma once

#include "Doom.h"

void AddToBox(Fixed* box, Fixed x, Fixed y);

uint32_t MiniLoop(
    void (*start)(),
    void (*stop)(),
    uint32_t (*ticker)(),
    void (*drawer)()
);

void D_DoomMain();
