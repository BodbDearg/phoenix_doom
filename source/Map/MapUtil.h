#pragma once

#include "Base/Angle.h"
#include "Base/Fixed.h"

struct line_t;
struct mobj_t;
struct subsector_t;
struct vector_t;

typedef bool (*BlockLinesIterCallback)(line_t&);
typedef bool (*BlockThingsIterCallback)(mobj_t&);

angle_t SlopeAngle(uint32_t num, uint32_t den) noexcept;
angle_t PointToAngle(Fixed x1, Fixed y1, Fixed x2, Fixed y2) noexcept;
Fixed GetApproxDistance(Fixed dx, Fixed dy) noexcept;
bool PointOnVectorSide(Fixed x, Fixed y, const vector_t& line) noexcept;
subsector_t& PointInSubsector(const Fixed x, const Fixed y) noexcept;
void MakeVector(line_t& li, vector_t& dl) noexcept;
Fixed InterceptVector(const vector_t& first, const vector_t& second) noexcept;
uint32_t LineOpening(const line_t& linedef) noexcept;
void UnsetThingPosition(mobj_t& thing) noexcept;
void SetThingPosition(mobj_t& thing) noexcept;
bool BlockLinesIterator(const uint32_t x, const uint32_t y, const BlockLinesIterCallback func) noexcept;
bool BlockThingsIterator(const uint32_t x, const uint32_t y, const BlockThingsIterCallback func) noexcept;
