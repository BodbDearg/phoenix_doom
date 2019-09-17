#pragma once

#include "Base/Angle.h"
#include "Base/Fixed.h"

struct mobj_t;
struct player_t;

extern mobj_t*  gpLineTarget;        // Object that was targeted
extern mobj_t*  gpTmpThing;          // mobj_t to be checked
extern Fixed    gTmpX;               // Temp x,y for a position to be checked
extern Fixed    gTmpY;
extern bool     gbCheckPosOnly;      // If true, just check the position, no actions
extern mobj_t*  gpShooter;           // Source of a direct line shot
extern angle_t  gAttackAngle;        // Angle to target
extern Fixed    gAttackRange;        // Range to target
extern Fixed    gAimTopSlope;        // Range of slope to target weapon
extern Fixed    gAimBottomSlope;

bool P_CheckPosition(mobj_t& thing, const Fixed x, const Fixed y) noexcept;
bool P_TryMove(mobj_t& thing, const Fixed x, const Fixed y) noexcept;
void P_UseLines(player_t& player) noexcept;
void RadiusAttack(mobj_t& spot, mobj_t* source, const uint32_t damage) noexcept;
Fixed AimLineAttack(mobj_t& t1, const angle_t angle, const Fixed distance) noexcept;

void LineAttack(
    mobj_t& t1,
    const angle_t angle,
    const Fixed distance,
    const Fixed slope,
    const uint32_t damage
) noexcept;
