#pragma once

#include <cstdint>

// Flags for PrintNumber
static constexpr uint32_t PNFLAGS_PERCENT   = 1;    // Percent sign appended?
static constexpr uint32_t PNFLAGS_CENTER    = 2;    // Use X as center and not left x
static constexpr uint32_t PNFLAGS_RIGHT     = 4;    // Use right justification

void PrintBigFont(uint32_t x, uint32_t y, const char* string);
uint32_t GetBigStringWidth(const char* string);
void PrintNumber(uint32_t x, uint32_t y, uint32_t value, uint32_t Flags);
void PrintBigFontCenter(uint32_t x, uint32_t y, const char* String);
void IN_Start();
void IN_Stop();
uint32_t IN_Ticker();
void IN_Drawer();
