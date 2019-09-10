#pragma once

struct player_t;

extern bool gShowAllAutomapThings;      // Cheat: if true, show all objects
extern bool gShowAllAutomapLines;       // Cheat: If true, show all lines

void AM_Start() noexcept;
void AM_Control(player_t& player) noexcept;
void AM_Drawer() noexcept;
