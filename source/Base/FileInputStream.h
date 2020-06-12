#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>

//------------------------------------------------------------------------------------------------------------------------------------------
// Byte oriented input stream for reading from a file on disk
//------------------------------------------------------------------------------------------------------------------------------------------
class FileInputStream {
public:
    class StreamException {};   // Thrown when there is a problem reading etc.

    FileInputStream() noexcept;
    FileInputStream(FileInputStream&& other) noexcept;
    ~FileInputStream() noexcept;

    // Note: if a file is already opened and an attempt is made to open another file then the current file is closed!
    bool isOpen() const noexcept;
    void open(const char* const pFilePath) THROWS;
    void close() noexcept;

    uint32_t size() const THROWS;
    uint32_t tell() const THROWS;
    void seek(const uint32_t offset) THROWS;
    void skip(const uint32_t numBytes) THROWS;
    void readBytes(std::byte* const pBytes, const uint32_t numBytes) THROWS;

    template <class T>
    inline void read(T& output) THROWS {
        readBytes(reinterpret_cast<std::byte*>(&output), sizeof(T));
    }

    template <class T>
    inline T read() THROWS {
        T output;
        read(output);
        return output;
    }

private:
    void* mpFile;
};
