#pragma once

#include <cstdint>

void ThreeDOMain() noexcept;
void WritePrefsFile();
void ClearPrefsFile();
void ReadPrefsFile();

// TODO: MOVE ELSEWHERE
void DrawPlaque(uint32_t RezNum);
