#pragma once

#include <cstdint>

void ThreeDOMain() noexcept;
void WritePrefsFile() noexcept;
void ClearPrefsFile() noexcept;
void ReadPrefsFile() noexcept;

// TODO: MOVE ELSEWHERE
void DrawPlaque(uint32_t RezNum) noexcept;
