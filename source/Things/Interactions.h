#pragma once

#include <cstdint>

struct mobj_t;

void TouchSpecialThing(mobj_t& special, mobj_t& toucher) noexcept;

void DamageMObj(
    mobj_t& target,
    mobj_t* const pInflictor,
    mobj_t* const pSource,
    uint32_t damage
) noexcept;
