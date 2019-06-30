#pragma once

#include "Doom.h"

extern line_t*  shootline;
extern mobj_t*  shootmobj;
extern Fixed    shootslope;     // Between aimtop and aimbottom
extern Fixed    shootx;         // Location for puff/blood
extern Fixed    shooty;
extern Fixed    shootz;

void P_Shoot2();
bool PA_DoIntercept(void* value, bool isline, Fixed frac);
bool PA_ShootLine(line_t* li, Fixed interceptfrac);
bool PA_ShootThing(mobj_t* th, Fixed interceptfrac);
Fixed PA_SightCrossLine(line_t* line);
bool PA_CrossSubsector(const subsector_t* sub);
