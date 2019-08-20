#pragma once

#include <cstdint>

// TODO: move elsewhere
extern void DrawARect(const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height, const uint16_t color) noexcept;

// Input handlers
static constexpr uint32_t PadDown       = 0x80000000;
static constexpr uint32_t PadUp         = 0x40000000;
static constexpr uint32_t PadRight      = 0x20000000;
static constexpr uint32_t PadLeft       = 0x10000000;
static constexpr uint32_t PadA          = 0x08000000;
static constexpr uint32_t PadB          = 0x04000000;
static constexpr uint32_t PadC          = 0x02000000;
static constexpr uint32_t PadD          = 0x00000000;
static constexpr uint32_t PadStart      = 0x01000000;
static constexpr uint32_t PadX          = 0x00800000;
static constexpr uint32_t PadRightShift = 0x00400000;
static constexpr uint32_t PadLeftShift  = 0x00200000;
static constexpr uint32_t PadXLeft      = 0x00100000;
static constexpr uint32_t PadXRight     = 0x00080000;

extern uint32_t ReadJoyButtons(uint32_t Which) noexcept;

// Misc routines
extern void LongWordToAscii(uint32_t Input, char* AsciiPtr) noexcept;
extern uint32_t SaveAFile(const char* FileName, void* data, uint32_t Length) noexcept;
