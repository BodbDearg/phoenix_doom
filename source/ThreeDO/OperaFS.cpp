#include "OperaFS.h"

#include "Base/Endian.h"
#include "Base/Finally.h"
#include <cstdio>

BEGIN_NAMESPACE(OperaFS)

struct IOException {};          // Thrown when there is a problem with IO
struct BadDataException {};     // Thrown when bad or unexpected data is encountered

// Expect this as the block size on all things
static constexpr uint32_t OPERA_BLOCK_SIZE = 2048;

//----------------------------------------------------------------------------------------------------------------------
// This is the header for the OperaFS disc
//----------------------------------------------------------------------------------------------------------------------
struct DiscHeader {
    // Expected header constants
    static constexpr uint8_t RECORD_TYPE = 1;
    static constexpr uint8_t VERSION = 1;
    static constexpr uint8_t SYNC_BYTE = 0x5A;
    
    uint32_t    _unknown[4];                // I'm not sure what these entries are, they are not mentioned in the 3DO SDK
	uint8_t     recordType;                 // Must be '1'
    uint8_t     syncBytes[5];               // All of these must be '0x5A'
    uint8_t     version;                    // Should be '1'
    uint8_t     flags;                      // Should be '0' (purpose unknown)
    char        comments[32];               // Comments for the volume
    char        label[32];                  // Label for the volume
    uint32_t    discId;                     // ID for the disc
    uint32_t    discBlockSize;              // Should be '2048'
    uint32_t    discNumBlocks;              // Number of blocks in the disc
    uint32_t    rootDirId;                  // ID for the root directory
    uint32_t    rootDirBlockCount;          // Number of blocks in the root dir
    uint32_t    rootDirBlockSize;           // Should be '2048'
    uint32_t    rootDirLastCopyIdx;         // The number of copies of the root directory minus 1. 0 = 1 copy, 1 = 2 copies etc.
    uint32_t    rootDirCopyOffsets[1];      // The offset (in blocks from the beginning of the disc) to each root dir copy. There may be a variable number of these but we only use the first!

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(discId);
        Endian::convertBigToHost(discBlockSize);
        Endian::convertBigToHost(discNumBlocks);
        Endian::convertBigToHost(rootDirId);
        Endian::convertBigToHost(rootDirBlockCount);
        Endian::convertBigToHost(rootDirBlockSize);
        Endian::convertBigToHost(rootDirLastCopyIdx);
        Endian::convertBigToHost(rootDirCopyOffsets[0]);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Helper functions for dealing with file I/O
//----------------------------------------------------------------------------------------------------------------------
template <class T>
static void fileRead(FILE* const pFile, T& out) THROWS {
    if (std::fread(&out, sizeof(T), 1, pFile) != 1)
        throw IOException();
}

static void fileSeek(FILE* const pFile, const uint32_t offset) THROWS {
    if (offset > LONG_MAX)
        throw IOException();

    if (std::fseek(pFile, (long) offset, SEEK_SET) != 0)
        throw IOException();
}

//----------------------------------------------------------------------------------------------------------------------
// Read and verify the OperaFS disc header
//----------------------------------------------------------------------------------------------------------------------
static void readAndVerifyDiscHeader(FILE* const pFile, DiscHeader& discHeader) THROWS {
    // Read and fix the endianness of the data
    fileRead(pFile, discHeader);
    discHeader.convertBigToHostEndian();

    // Verify the header
    const bool bValidHeader = (
        (discHeader.recordType == DiscHeader::RECORD_TYPE) &&
        (discHeader.syncBytes[0] == DiscHeader::SYNC_BYTE) &&
        (discHeader.syncBytes[1] == DiscHeader::SYNC_BYTE) &&
        (discHeader.syncBytes[2] == DiscHeader::SYNC_BYTE) &&
        (discHeader.syncBytes[3] == DiscHeader::SYNC_BYTE) &&
        (discHeader.syncBytes[4] == DiscHeader::SYNC_BYTE) &&
        (discHeader.version == DiscHeader::VERSION) &&
        (discHeader.discBlockSize == OPERA_BLOCK_SIZE) &&
        (discHeader.discNumBlocks >= 1) &&
        (discHeader.rootDirBlockCount >= 1) &&
        (discHeader.rootDirBlockSize == OPERA_BLOCK_SIZE) &&
        (discHeader.rootDirCopyOffsets[0] < discHeader.discNumBlocks)   // Make sure the first dir entry for the root is at a valid block offset!
    );

    if (!bValidHeader)
        throw BadDataException();
}

bool getFSEntriesFromDiscImage(const char* const pDiscImagePath, std::vector<FSEntry>& fsEntries) noexcept {
    // Ensure the input list is clear and try to open the disc image file
    fsEntries.clear();
    FILE* const pFile = std::fopen(pDiscImagePath, "rb");

    if (!pFile)
        return false;
    
    auto cleanupFile = finally([&]() {
        std::fclose(pFile);
    });

    // Do the reading
    try {
        // Read the disc header first
        DiscHeader discHeader;
        readAndVerifyDiscHeader(pFile, discHeader);

        // All good if we got to here!
        return true;
    }
    catch (...) {
        // Failed to read the filesystem!
        fsEntries.clear();
        return false;
    }
}

END_NAMESPACE(OperaFS)
