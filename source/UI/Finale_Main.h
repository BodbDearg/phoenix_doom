#pragma once

#include <cstdint>

enum gameaction_e : uint8_t;

void F_Start() noexcept;
void F_Stop() noexcept;
gameaction_e F_Ticker() noexcept;
void F_Drawer(const bool bSaveFrameBuffer) noexcept;
