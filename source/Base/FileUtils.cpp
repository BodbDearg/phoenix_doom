#include "FileUtils.h"

#include "Finally.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

BEGIN_NAMESPACE(FileUtils)

bool getContentsOfFile(
    const char* const filePath,
    std::byte*& pOutputMem,
    size_t& outputSize,
    const size_t numExtraBytes,
    const std::byte extraBytesValue
) noexcept {
    // Clear output firstly
    pOutputMem = nullptr;
    outputSize = 0;

    // Open the file firstly and ensure it will be closed on exit
    ASSERT(filePath);
    FILE* pFile = std::fopen(filePath, "rb");

    if (!pFile) {
        return false;
    }

    auto closeFile = finally([&]() noexcept {
        std::fclose(pFile);
    });

    // Figure out what size it is and rewind back to the start
    if (std::fseek(pFile, 0, SEEK_END) != 0)
        return false;
    
    const long fileSize = std::ftell(pFile);

    if (fileSize <= 0 || fileSize >= INT32_MAX)
        return false;
    
    if (std::fseek(pFile, 0, SEEK_SET) != 0)
        return false;
    
    // Try to read the file contents
    std::byte* const pTmpBuffer = new std::byte[fileSize + numExtraBytes];

    if (std::fread(pTmpBuffer, (uint32_t) fileSize, 1, pFile) != 1) {
        delete[] pTmpBuffer;
        return false;
    }

    // Success! Set the value of the extra bytes (if specified)
    if (numExtraBytes > 0) {
        std::memset(pTmpBuffer + fileSize, (int) extraBytesValue, numExtraBytes);
    }

    // Save the result and return 'true' for success
    pOutputMem = pTmpBuffer;
    outputSize = (size_t) fileSize;
    return true;
}

END_NAMESPACE(FileUtils)
