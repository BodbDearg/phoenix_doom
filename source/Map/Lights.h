#pragma once

#include "Base/MathUtils.h"

struct line_t;
struct sector_t;

static constexpr uint32_t GLOWSPEED = convertPcUint32Speed(8);      // Steps for a glowing light
static constexpr uint32_t STROBEBRIGHT = convertPcTicks(5);         // Time to increase brightness for strobe
static constexpr uint32_t FASTDARK = convertPcTicks(15);            // Ticks to glow quickly
static constexpr uint32_t SLOWDARK = convertPcTicks(35);            // Ticks to glow slowly

void P_SpawnLightFlash(sector_t& sector) noexcept;
void P_SpawnStrobeFlash(sector_t& sector, const uint32_t fastOrSlow, const bool bInSync) noexcept;
void EV_StartLightStrobing(line_t& line) noexcept;
void EV_TurnTagLightsOff(line_t& line) noexcept;
void EV_LightTurnOn(line_t& line, const uint32_t bright) noexcept;
void P_SpawnGlowingLight(sector_t& sector) noexcept;
