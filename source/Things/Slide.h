#pragma once

#include "Base/Macros.h"
#include "Base/Fixed.h"

struct line_t;
struct mobj_t;

BEGIN_NAMESPACE(Slide)

extern Fixed    gSlideX;            // The final position
extern Fixed    gSlideY;
extern line_t*  gpSpecialLine;      // A line we may have activated

void init() noexcept;
void shutdown() noexcept;
void doSliding(mobj_t& mo) noexcept;

END_NAMESPACE(Slide)
