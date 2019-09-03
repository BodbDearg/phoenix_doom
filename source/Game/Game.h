#pragma once

#include <cstdint>

enum skill_e : uint8_t;

void G_DoLoadLevel() noexcept;
void G_PlayerFinishLevel() noexcept;
void G_PlayerReborn() noexcept;
void G_DoReborn() noexcept;
void G_ExitLevel() noexcept;
void G_SecretExitLevel() noexcept;
void G_InitNew(const skill_e skill, const uint32_t map) noexcept;
void G_RunGame() noexcept;
