#pragma once

#include "Base/Fixed.h"

struct line_t;
struct mobj_t;

extern bool     gbTryMove2;         // Result from P_TryMove2
extern bool     gbFloatOk;          // If true, move would be ok if within tmfloorz - tmceilingz
extern Fixed    gTmpFloorZ;         // Current floor z for P_TryMove2
extern Fixed    gTmpCeilingZ;       // Current ceiling z for P_TryMove2
extern mobj_t*  gpMoveThing;        // Either a skull/missile target or a special pickup
extern line_t*  gpBlockLine;        // Might be a door that can be opened

void P_TryMove2() noexcept;
void PM_CheckPosition() noexcept;
bool PM_BoxCrossLine(line_t& ld) noexcept;
bool PIT_CheckLine(line_t& ld) noexcept;
bool PIT_CheckThing(mobj_t& thing) noexcept;
