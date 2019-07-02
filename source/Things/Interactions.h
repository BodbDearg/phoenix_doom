#pragma once

#include <cstdint>

struct mobj_t;

void TouchSpecialThing(mobj_t* special, mobj_t* toucher);
void DamageMObj(mobj_t* target, mobj_t* inflictor, mobj_t* source, uint32_t damage);
