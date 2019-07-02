#pragma once

#include "Base/Fixed.h"

struct line_t;
struct mobj_t;

extern bool     trymove2;       // Result from P_TryMove2
extern bool     floatok;        // If true, move would be ok if within tmfloorz - tmceilingz
extern Fixed    tmfloorz;       // Current floor z for P_TryMove2
extern Fixed    tmceilingz;     // Current ceiling z for P_TryMove2
extern mobj_t*  movething;      // Either a skull/missile target or a special pickup
extern line_t*  blockline;      // Might be a door that can be opened

void P_TryMove2();
void PM_CheckPosition();
bool PM_BoxCrossLine(line_t* ld);
bool PIT_CheckLine(line_t* ld);
uint32_t PIT_CheckThing(mobj_t* thing);
