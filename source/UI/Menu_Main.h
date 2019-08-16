#pragma once

#include <cstdint>

enum gameaction_e : uint8_t;

void M_Start() noexcept;
void M_Stop() noexcept;
gameaction_e M_Ticker() noexcept;
void M_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept;
