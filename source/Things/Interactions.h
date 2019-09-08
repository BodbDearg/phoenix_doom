#pragma once

#include "Player.h"

struct mobj_t;
struct player_t;

uint32_t GivePower(player_t& player, const powertype_e power) noexcept;
void givePlayerABackpack(player_t& player) noexcept;
void TouchSpecialThing(mobj_t& special, mobj_t& toucher) noexcept;

void DamageMObj(
    mobj_t& target,
    mobj_t* const pInflictor,
    mobj_t* const pSource,
    uint32_t damage
) noexcept;
