#pragma once

#include "Base/FileInputStream.h"

//------------------------------------------------------------------------------------------------------------------------------------------
// Allows reading of data from a raw image of a CD-ROM stored in a file on disk.
//
// Supports the following CD-ROM sector modes (ONLY!):
//      - Mode 1 / 2352 (what 3DO DOOM uses)
//      - Mode 2 / 2352
//
// Basically this class abstracts the process of reading data so that the contents of the disc can be read without
// consideration for the underlying CD-ROM sector format. For example if you ask this class to seek to byte 50000 then
// that will be byte 50000 of the actual disc user data, and you will not have to make any consideration for CD-ROM
// sector overhead etc.
//------------------------------------------------------------------------------------------------------------------------------------------
class CDImageFileInputStream {
public:
    struct StreamException {};  // Thrown in some situations

    CDImageFileInputStream() noexcept;
    CDImageFileInputStream(CDImageFileInputStream&& other) noexcept;
    ~CDImageFileInputStream() noexcept;

    // Note: if a disc image is already opened and an attempt is made to open another image then the current image is closed!
    bool isOpen() const noexcept;
    void open(const char* const pFilePath) THROWS;
    void close() noexcept;

    uint32_t size() const THROWS;
    uint32_t tell() const;
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
    FileInputStream     mFileStream;
    uint32_t            mUserBytesPerSector;    // How much actual data per CD sector - differs depending on CD mode (2048 for mode1, 2336 for mode2)
    uint32_t            mSectorEndSkipBytes;    // How much control data to skip at the end of each CD sector (288 for mode1, 0 for mode2)
    uint32_t            mCurDataOffset;         // Current offset into the actual disc data
};
