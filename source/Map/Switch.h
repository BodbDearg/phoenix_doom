#pragma once

#include <cstdint>

struct line_t;
struct mobj_t;

extern uint32_t NumSwitches;    // Number of switches * 2
extern uint32_t SwitchList[];

void P_InitSwitchList();
void P_ChangeSwitchTexture(line_t* line, bool useAgain);
bool P_UseSpecialLine(mobj_t* thing, line_t* line);
