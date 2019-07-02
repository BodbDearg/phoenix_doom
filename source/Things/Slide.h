#pragma once

#include "Game/Doom.h"

struct line_t;
struct mobj_t;

extern Fixed    slidex;         // The final position
extern Fixed    slidey;
extern line_t*  specialline;

void P_SlideMove(mobj_t* mo);
Fixed P_CompletableFrac(Fixed dx, Fixed dy);
int32_t SL_PointOnSide(int32_t x, int32_t y);
Fixed SL_CrossFrac();
bool CheckLineEnds();
void ClipToLine();
uint32_t SL_CheckLine(line_t* ld);
void SL_CheckSpecialLines(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
