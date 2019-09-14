#pragma once

#include <cstdint>

enum gameaction_e : uint8_t;

void IN_Start() noexcept;
void IN_Stop() noexcept;
gameaction_e IN_Ticker() noexcept;
void IN_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept;
