#include "CDImageFileInputStream.h"

#include <cstring>
#include <type_traits>

// Expected CD-ROM sector header sync pattern
static constexpr uint8_t CDROM_SECTOR_SYNC_PATTERN[12] = {
    0x00u, 0xFFu, 0xFFu, 0xFFu,
    0xFFu, 0xFFu, 0xFFu, 0xFFu,
    0xFFu, 0xFFu, 0xFFu, 0x00u
};

// CD-ROM sector header struct
struct alignas(1) CDSectorHeader {
    uint8_t syncPattern[12];    // Should be 0x00FFFFFF, 0xFFFFFFFF, 0xFFFFFF00
    uint8_t address[3];
    uint8_t mode;

    bool hasValidSyncPattern() const noexcept {
        return (std::memcmp(syncPattern, CDROM_SECTOR_SYNC_PATTERN, sizeof(CDROM_SECTOR_SYNC_PATTERN)) == 0);
    }
};

// CD-ROM constants
static constexpr uint32_t CD_SECTOR_SIZE                    = 2352;
static constexpr uint32_t CD_SECTOR_HEADER_SIZE             = sizeof(CDSectorHeader);
static constexpr uint32_t CD_MODE1_SECTOR_USER_DATA_SIZE    = 2048;
static constexpr uint32_t CD_MODE2_SECTOR_USER_DATA_SIZE    = 2336;
static constexpr uint32_t CD_MODE1_SECTOR_END_SKIP_BYTES    = 288;
static constexpr uint32_t CD_MODE2_SECTOR_END_SKIP_BYTES    = 0;
static constexpr uint32_t CD_MAX_SECTORS                    = 445500;  // For a 90 minute CD-ROM which is the max ever (also very uncommon)

CDImageFileInputStream::CDImageFileInputStream() noexcept
    : mFileStream()
    , mUserBytesPerSector(0)
    , mSectorEndSkipBytes(0)
    , mCurDataOffset(0)
{
}

CDImageFileInputStream::CDImageFileInputStream(CDImageFileInputStream&& other) noexcept
    : mFileStream(std::move(other.mFileStream))
    , mUserBytesPerSector(other.mUserBytesPerSector)
    , mSectorEndSkipBytes(other.mSectorEndSkipBytes)
    , mCurDataOffset(other.mCurDataOffset)
{
    other.mUserBytesPerSector = 0;
    other.mSectorEndSkipBytes = 0;
    other.mCurDataOffset = 0;
}

CDImageFileInputStream::~CDImageFileInputStream() noexcept {
    // Don't need to do anything...
}

bool CDImageFileInputStream::isOpen() const noexcept {
    return mFileStream.isOpen();
}

void CDImageFileInputStream::open(const char* const pFilePath) THROWS {
    close();
    mFileStream.open(pFilePath);

    // Determine CD sector mode by reading the sector header
    try {
        // Read the sector header and ensure it has a valid sync pattern
        CDSectorHeader sectorHeader;
        mFileStream.read(sectorHeader);

        if (!sectorHeader.hasValidSyncPattern()) 
            throw StreamException();

        // Determine CD sector mode and the number of actual data bytes per sector
        if (sectorHeader.mode == 1) {
            mUserBytesPerSector = CD_MODE1_SECTOR_USER_DATA_SIZE;
            mSectorEndSkipBytes = CD_MODE1_SECTOR_END_SKIP_BYTES;
        }
        else if (sectorHeader.mode == 2) {
            mUserBytesPerSector = CD_MODE2_SECTOR_USER_DATA_SIZE;
            mSectorEndSkipBytes = CD_MODE2_SECTOR_END_SKIP_BYTES;
        }
        else {
            throw StreamException();    // Bad or unsupported CD-ROM format
        }

        // Note that after reading the sector header (16 bytes) we already point to the first user data byte!
        // Therefore no actual seek required.
        mCurDataOffset = 0;
    }
    catch (...) {
        close();
        throw FileInputStream::StreamException();
    }
}

void CDImageFileInputStream::close() noexcept {
    mFileStream.close();
    mUserBytesPerSector = 0;
    mSectorEndSkipBytes = 0;
    mCurDataOffset = 0;
}

uint32_t CDImageFileInputStream::size() const THROWS {
    ASSERT(isOpen());

    // Note: if for some reason the file contains a partial sector at the end then it will be
    // ignored for the purposes of the size calculation!
    const uint32_t imageSize = mFileStream.size();
    const uint32_t numSectors = imageSize / CD_SECTOR_SIZE;
    const uint32_t userDataSize = numSectors * mUserBytesPerSector;
    return userDataSize;
}

uint32_t CDImageFileInputStream::tell() const {
    return mCurDataOffset;
}

void CDImageFileInputStream::seek(const uint32_t offset) THROWS {
    ASSERT(isOpen());

    // Compute the actual offset in the underlying raw CD-ROM data
    const uint32_t sectorNum = offset / mUserBytesPerSector;

    if (sectorNum > CD_MAX_SECTORS)
        throw StreamException();
    
    const uint32_t offsetInSector = offset - sectorNum * mUserBytesPerSector;
    const uint32_t realOffset = sectorNum * CD_SECTOR_SIZE + CD_SECTOR_HEADER_SIZE + offsetInSector;

    // Seek and save the new offset if successful
    mFileStream.seek(realOffset);
    mCurDataOffset = offset;
}

void CDImageFileInputStream::skip(const uint32_t numBytes) THROWS {
    const uint32_t maxPossibleBytesLeft = UINT32_MAX - mCurDataOffset;

    if (numBytes > maxPossibleBytesLeft)
        throw StreamException();
    
    seek(mCurDataOffset + numBytes);
}

void CDImageFileInputStream::readBytes(std::byte* const pBytes, const uint32_t numBytes) THROWS {    
    ASSERT(isOpen());

    std::byte* pCurBytes = pBytes;
    uint32_t numBytesLeft = numBytes;

    while (numBytesLeft > 0) {
        const uint32_t sectorBytesLeft = mUserBytesPerSector - mCurDataOffset % mUserBytesPerSector;

        // See if there are enough bytes left in the sector to fulfill the remaining bytes to be read.
        // If not then we will have to split the read across multiple sectors:
        if (numBytesLeft > sectorBytesLeft) {
            mFileStream.readBytes(pCurBytes, sectorBytesLeft);
            mCurDataOffset += sectorBytesLeft;
            pCurBytes += sectorBytesLeft;
            numBytesLeft -= sectorBytesLeft;
            
            // Skip any control bytes at the end of the sector and the header at the beginning of the next sector
            mFileStream.skip(mSectorEndSkipBytes + CD_SECTOR_HEADER_SIZE);
        } else {
            mFileStream.readBytes(pCurBytes, numBytesLeft);
            mCurDataOffset += numBytesLeft;
            numBytesLeft = 0;
        }
    }
}
