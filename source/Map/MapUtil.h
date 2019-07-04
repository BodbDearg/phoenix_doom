#pragma once

#include "Base/Angle.h"
#include "Base/Fixed.h"

struct line_t;
struct mobj_t;
struct subsector_t;
struct vector_t;

angle_t SlopeAngle(uint32_t num, uint32_t den);
angle_t PointToAngle(Fixed x1, Fixed y1, Fixed x2, Fixed y2);
Fixed GetApproxDistance(Fixed dx, Fixed dy);
uint32_t PointOnVectorSide(Fixed x, Fixed y, const vector_t* line);
subsector_t* PointInSubsector(Fixed x, Fixed y);
void MakeVector(line_t* li,vector_t* dl);
Fixed InterceptVector(vector_t* v2, vector_t* v1);
uint32_t LineOpening(line_t* linedef);
void UnsetThingPosition(mobj_t* thing);
void SetThingPosition(mobj_t* thing);
uint32_t BlockLinesIterator(uint32_t x, uint32_t y, uint32_t(*func)(line_t*));
uint32_t BlockThingsIterator(uint32_t x, uint32_t y, uint32_t(*func)(mobj_t*));
