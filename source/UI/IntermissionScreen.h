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
void PrintBigFont(const int32_t x, const int32_t y, const char* const pString) noexcept;
uint32_t GetBigStringWidth(const char* const pString) noexcept;
void PrintNumber(const int32_t x, const int32_t y, const uint32_t value, const uint32_t flags) noexcept;
void PrintBigFontCenter(const int32_t x, const int32_t y, const char* const str) noexcept;

void IN_Start() noexcept;
void IN_Stop() noexcept;
gameaction_e IN_Ticker() noexcept;
void IN_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept;
