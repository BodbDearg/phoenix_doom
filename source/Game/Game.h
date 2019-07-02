#pragma once

#include <cstdint>

enum skill_e : uint8_t;

void G_DoLoadLevel();
void G_PlayerFinishLevel();
void G_PlayerReborn();
void G_DoReborn();
void G_ExitLevel();
void G_SecretExitLevel();
void G_InitNew(skill_e skill, uint32_t map);
void G_RunGame();
uint32_t G_PlayDemoPtr(uint32_t* demo);
void G_RecordDemo();
