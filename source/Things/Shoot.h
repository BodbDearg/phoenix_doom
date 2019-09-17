#pragma once

#include "Base/Fixed.h"

struct line_t;
struct mobj_t;
struct subsector_t;
struct vertex_t;

extern line_t*  gpShootLine;
extern mobj_t*  gpShootMObj;
extern Fixed    gShootSlope;     // Between aimtop and aimbottom
extern Fixed    gShootX;         // Location for puff/blood
extern Fixed    gShootY;
extern Fixed    gShootZ;

void P_Shoot2() noexcept;
bool PA_DoIntercept(void* pValue, bool isLine, Fixed frac) noexcept;
bool PA_ShootLine(line_t& li, const Fixed interceptfrac) noexcept;
bool PA_ShootThing(mobj_t& th, const Fixed interceptfrac) noexcept;
Fixed PA_SightCrossLine(const vertex_t& lineV1, const vertex_t& lineV2) noexcept;
Fixed PA_SightCrossLine(const line_t& line) noexcept;
bool PA_CrossSubsector(const subsector_t& sub) noexcept;
