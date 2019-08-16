#pragma once

#include <cstdint>

enum gameaction_e : uint8_t;

// TODO: These should probably live somewhere else
//
// Flags for PrintNumber
static constexpr uint32_t PNFLAGS_PERCENT   = 1;    // Percent sign appended?
static constexpr uint32_t PNFLAGS_CENTER    = 2;    // Use X as center and not left x
static constexpr uint32_t PNFLAGS_RIGHT     = 4;    // Use right justification

// TODO: These should probably live somewhere else
void PrintBigFont(int32_t x, int32_t y, const char* string) noexcept;
uint32_t GetBigStringWidth(const char* string) noexcept;
void PrintNumber(int32_t x, int32_t y, uint32_t value, uint32_t Flags) noexcept;
void PrintBigFontCenter(const int32_t x, const int32_t y, const char* const str) noexcept;

void IN_Start() noexcept;
void IN_Stop() noexcept;
gameaction_e IN_Ticker() noexcept;
void IN_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept;
