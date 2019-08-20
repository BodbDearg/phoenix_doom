#pragma once

#include <cstdint>

struct mobj_t;

bool CheckSight(mobj_t& t1, mobj_t& t2) noexcept;
