#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Game Data File System: provides access to the assets used by the game.
// Abstracts the process so that the game data can transparently be retrieved from either a CD-ROM image of 3DO Doom or
// a folder on disk containing the extracted contents of the CD.
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(GameDataFS)

void init() noexcept;
void shutdown() noexcept;

// Exact same functionality as the same function in 'FileUtils', except abstracted to work with whatever type of
// filesystem the game is configured to use.
bool getContentsOfFile(
    const char* const pFilePath,
    std::byte*& pOutputMem,
    size_t& outputSize,
    const size_t numExtraBytes = 0,
    const std::byte extraBytesValue = std::byte(0)
) noexcept;

END_NAMESPACE(GameDataFS)
