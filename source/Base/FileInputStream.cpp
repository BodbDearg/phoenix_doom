#include "FileInputStream.h"
#include <cstdio>

static inline int64_t u32ToI64NoSignExtend(const uint32_t val) noexcept {
    return (int64_t)(uint64_t) val;
}

static inline void ensureU32ConvertibleToLong(const uint32_t val) THROWS {
    if constexpr (sizeof(long) <= sizeof(uint32_t)) {
        const int64_t valI64 = u32ToI64NoSignExtend(val);

        if (valI64 > LONG_MAX)
            throw FileInputStream::StreamException();
    }
}

static inline void ensurePositiveLongConvertibleToU32(const long val) THROWS {
    ASSERT(val >= 0);

    if constexpr (sizeof(long) > sizeof(uint32_t)) {
        if (val > u32ToI64NoSignExtend(UINT32_MAX))
            throw FileInputStream::StreamException();
    }
}

FileInputStream::FileInputStream() noexcept
    : mpFile(nullptr)
{
}

FileInputStream::FileInputStream(FileInputStream&& other) noexcept
    : mpFile(other.mpFile)
{
    other.mpFile = nullptr;
}

FileInputStream::~FileInputStream() noexcept {
    close();
}

bool FileInputStream::isOpen() const noexcept {
    return (mpFile != nullptr);
}

void FileInputStream::open(const char* const pFilePath) THROWS {
    close();
    mpFile = std::fopen(pFilePath, "rb");

    if (!mpFile)
        throw StreamException();
}

void FileInputStream::close() noexcept {
    if (mpFile) {
        std::fclose((FILE*) mpFile);
        mpFile = nullptr;
    }
}

uint32_t FileInputStream::size() const THROWS {
    ASSERT(mpFile);

    // Seek to the end and get the offset to tell the size, then restore the previous position
    const uint32_t prevOffset = tell();

    if (std::fseek((FILE*) mpFile, 0, SEEK_END) != 0)
        throw StreamException();
    
    const long fileOffset = std::ftell((FILE*) mpFile);

    if (std::fseek((FILE*) mpFile, (long) prevOffset, SEEK_SET) != 0)
        throw StreamException();
    
    if (fileOffset < 0)
        throw StreamException();
    
    ensurePositiveLongConvertibleToU32(fileOffset);
    return (uint32_t) fileOffset;
}

uint32_t FileInputStream::tell() const THROWS {
    ASSERT(mpFile);    
    const long fileOffset = std::ftell((FILE*) mpFile);

    if (fileOffset < 0)
        throw StreamException();
    
    ensurePositiveLongConvertibleToU32(fileOffset);
    return (uint32_t) fileOffset;
}

void FileInputStream::seek(const uint32_t offset) THROWS {
    ASSERT(mpFile);
    ensureU32ConvertibleToLong(offset);
    
    if (std::fseek((FILE*) mpFile, (long) offset, SEEK_SET) != 0)
        throw StreamException();
}

void FileInputStream::skip(const uint32_t numBytes) THROWS {
    ASSERT(mpFile);
    ensureU32ConvertibleToLong(numBytes);
    
    if (std::fseek((FILE*) mpFile, (long) numBytes, SEEK_CUR) != 0)
        throw StreamException();
}

void FileInputStream::readBytes(std::byte* const pBytes, const uint32_t numBytes) THROWS {
    ASSERT(mpFile);

    if (numBytes == 0)
        return;
    
    if (std::fread(pBytes, numBytes, 1, (FILE*) mpFile) != 1)
        throw StreamException();
}
