#pragma once

#include "Base/Fixed.h"

struct line_t;
struct mobj_t;

extern Fixed    gSlideX;    // The final position
extern Fixed    gSlideY;
extern line_t*  gpSpecialLine;

void P_SlideMove(mobj_t& mo) noexcept;
Fixed P_CompletableFrac(const Fixed dx, const Fixed dy) noexcept;
uint32_t SL_PointOnSide(const Fixed x, const Fixed y) noexcept;
Fixed SL_CrossFrac() noexcept;
bool CheckLineEnds() noexcept;
void ClipToLine() noexcept;
bool SL_CheckLine(line_t& ld) noexcept;
void SL_CheckSpecialLines(
    const int32_t x1,
    const int32_t y1,
    const int32_t x2,
    const int32_t y2
) noexcept;
