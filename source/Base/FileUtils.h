#pragma once

#include "Macros.h"
#include <cstddef>

BEGIN_NAMESPACE(FileUtils)

/**
 * Read data for the given file path on disk and store it in the given pointer, returning 'true' on success.
 * Optionally an additional number of bytes at the end of the data can be allocated and set to the given value.
 * This can be useful to null terminate a text file that has been read, for example.
 *
 * Notes:
 *  (1) If the file is not read successfully, the given filepath will be set to null and output size set to '0'.
 *  (2) The output memory is allocated with C++ 'new[]' so should be deallocated with C++ 'delete[]'.
 *  (3) The output size does NOT include the extra bytes added.
 */
bool getContentsOfFile(
    const char* const filePath,
    std::byte*& pOutputMem,
    size_t& outputSize,
    const size_t numExtraBytes = 0,
    const std::byte extraBytesValue = std::byte(0)
) noexcept;

/**
 * Write the specified block of bytes to the given file.
 * Returns 'true' on success.
 */
bool writeDataToFile(
    const char* const filePath,
    const std::byte* const pData,
    const size_t dataSize,
    const bool bAppend = false
) noexcept;

END_NAMESPACE(FileUtils)
