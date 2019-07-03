#pragma once

#include "Base/Angle.h"
#include "Base/Fixed.h"

struct mobj_t;
struct player_t;

extern mobj_t*  gLineTarget;         // Object that was targeted
extern mobj_t*  gTmpThing;           // mobj_t to be checked
extern Fixed    gTmpX;               // Temp x,y for a position to be checked
extern Fixed    gTmpY;
extern bool     gCheckPosOnly;       // If true, just check the position, no actions
extern mobj_t*  gShooter;            // Source of a direct line shot
extern angle_t  gAttackAngle;        // Angle to target
extern Fixed    gAttackRange;        // Range to target
extern Fixed    gAimTopSlope;        // Range of slope to target weapon
extern Fixed    gAimBottomSlope;

bool P_CheckPosition(mobj_t* thing, Fixed x, Fixed y);
bool P_TryMove(mobj_t* thing, Fixed x, Fixed y);
void P_UseLines(player_t* player);
void RadiusAttack(mobj_t* spot, mobj_t* source, uint32_t damage);
Fixed AimLineAttack(mobj_t* t1, angle_t angle, Fixed distance);
void LineAttack(mobj_t* t1, angle_t angle, Fixed distance, Fixed slope, uint32_t damage);
