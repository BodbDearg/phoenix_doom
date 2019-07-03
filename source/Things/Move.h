#pragma once

#include "Base/Fixed.h"

struct line_t;
struct mobj_t;

extern bool     gTryMove2;      // Result from P_TryMove2
extern bool     gFloatOk;       // If true, move would be ok if within tmfloorz - tmceilingz
extern Fixed    gTmpFloorZ;     // Current floor z for P_TryMove2
extern Fixed    gTmpCeilingZ;   // Current ceiling z for P_TryMove2
extern mobj_t*  gMoveThing;     // Either a skull/missile target or a special pickup
extern line_t*  gBlockLine;     // Might be a door that can be opened

void P_TryMove2();
void PM_CheckPosition();
bool PM_BoxCrossLine(line_t* ld);
bool PIT_CheckLine(line_t* ld);
uint32_t PIT_CheckThing(mobj_t* thing);
