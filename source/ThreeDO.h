#pragma once

#include <cstdint>

extern uint32_t gLastTics;      // Time elapsed since last page flip

void ThreeDOMain();
void WritePrefsFile();
void ClearPrefsFile();
void ReadPrefsFile();
void DrawPlaque(uint32_t RezNum);
