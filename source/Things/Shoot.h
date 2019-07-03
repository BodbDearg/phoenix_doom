#pragma once

#include "Base/Fixed.h"

struct line_t;
struct mobj_t;
struct subsector_t;

extern line_t*  gShootLine;
extern mobj_t*  gShootMObj;
extern Fixed    gShootSlope;     // Between aimtop and aimbottom
extern Fixed    gShootX;         // Location for puff/blood
extern Fixed    gShootY;
extern Fixed    gShootZ;

void P_Shoot2();
bool PA_DoIntercept(void* value, bool isline, Fixed frac);
bool PA_ShootLine(line_t* li, Fixed interceptfrac);
bool PA_ShootThing(mobj_t* th, Fixed interceptfrac);
Fixed PA_SightCrossLine(line_t* line);
bool PA_CrossSubsector(const subsector_t* sub);
