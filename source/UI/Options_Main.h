#pragma once

struct player_t;

void O_Init() noexcept;
void O_Control(player_t* player) noexcept;
void O_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept;
