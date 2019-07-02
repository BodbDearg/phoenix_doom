#pragma once

#include "Game/Doom.h"

struct mobj_t;
struct player_t;

extern mobj_t*  linetarget;         // Object that was targeted
extern mobj_t*  tmthing;            // mobj_t to be checked
extern Fixed    tmx;                // Temp x,y for a position to be checked
extern Fixed    tmy;
extern bool     checkposonly;       // If true, just check the position, no actions
extern mobj_t*  shooter;            // Source of a direct line shot
extern angle_t  attackangle;        // Angle to target
extern Fixed    attackrange;        // Range to target
extern Fixed    aimtopslope;        // Range of slope to target weapon
extern Fixed    aimbottomslope;

bool P_CheckPosition(mobj_t* thing, Fixed x, Fixed y);
bool P_TryMove(mobj_t* thing, Fixed x, Fixed y);
void P_UseLines(player_t* player);
void RadiusAttack(mobj_t* spot, mobj_t* source, uint32_t damage);
Fixed AimLineAttack(mobj_t* t1, angle_t angle, Fixed distance);
void LineAttack(mobj_t* t1, angle_t angle, Fixed distance, Fixed slope, uint32_t damage);
