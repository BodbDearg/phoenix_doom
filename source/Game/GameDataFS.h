#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>
#include <memory>

//----------------------------------------------------------------------------------------------------------------------
// Game Data File System: provides access to the assets used by the game.
// Abstracts the process so that the game data can transparently be retrieved from either a CD-ROM image of 3DO Doom or
// a folder on disk containing the extracted contents of the CD.
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(GameDataFS)

class InputStream;

void init() noexcept;
void shutdown() noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Exact same functionality as the same function in 'FileUtils', except abstracted to work with whatever type of
// filesystem the game is configured to use.
//----------------------------------------------------------------------------------------------------------------------
bool getContentsOfFile(
    const char* const pFilePath,
    std::byte*& pOutputMem,
    size_t& outputSize,
    const size_t numExtraBytes = 0,
    const std::byte extraBytesValue = std::byte(0)
) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Opens a game file for reading.
// The file closes itself automatically upon deletion.
//----------------------------------------------------------------------------------------------------------------------
std::unique_ptr<InputStream> openFile(const char* const pFilePath) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// An abstracted file input stream that reads game data from either a file on disk or a file embedded in a CD-ROM image.
// The file is closed (and can only be closed) by simply destroying it - you must cleanup the returned stream fully!
//----------------------------------------------------------------------------------------------------------------------
class InputStream {
public:
    virtual uint32_t size() const THROWS = 0;
    virtual uint32_t tell() const THROWS = 0;
    virtual void seek(const uint32_t offset) THROWS = 0;
    virtual void skip(const uint32_t numBytes) THROWS = 0;
    virtual void readBytes(std::byte* const pBytes, const uint32_t numBytes) THROWS = 0;

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

    inline virtual ~InputStream() noexcept {}
};

END_NAMESPACE(GameDataFS)
