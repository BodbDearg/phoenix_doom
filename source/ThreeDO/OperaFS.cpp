#include "OperaFS.h"

#include "Base/Endian.h"
#include "CDImageFileInputStream.h"
#include <queue>

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
// Header for a directory block in a OperaFS disc
//----------------------------------------------------------------------------------------------------------------------
struct DirBlockHeader {
    // Offset in blocks to the next block in the directory (0xFFFFFFFF or -1 if none).
    // Note: this is relative to the current directory block index!
    uint32_t nextBlock;

    // Offset in blocks to the previous block in the directory (0xFFFFFFFF or -1 if none).
    // Note: this is relative to the current directory block index!
    uint32_t prevBlock;

    // Flags for the directory block.
    // The exact meaning of this field is unknown (may be unused?).
    uint32_t flags;

    // Offset in bytes from the start of this block to the first unused byte in the block.
    // Can be used to determine the number of directory entries in the block.
    uint32_t firstFreeByte;

    // Offset in bytes from the start of the block to the first directory entry.
    // Can be used to determine the number of directory entries in the block.
    // This should always be set to '20' in reality, since that is the size of the directory block header.
    uint32_t firstByte;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(nextBlock);
        Endian::convertBigToHost(prevBlock);
        Endian::convertBigToHost(flags);
        Endian::convertBigToHost(firstFreeByte);
        Endian::convertBigToHost(firstByte);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Directory entry in a OperaFS disc
//----------------------------------------------------------------------------------------------------------------------
struct DirEntry {
    // Flags for directory entries.
    // Note: all of the bits must be set in each case for the flag to be considered true.
    static constexpr uint32_t FLAGS_FILE                    = 0x00000002;
    static constexpr uint32_t FLAGS_SPECIAL                 = 0x00000006;
    static constexpr uint32_t FLAGS_DIR                     = 0x00000007;
    static constexpr uint32_t FLAGS_LAST_ENTRY_IN_BLOCK     = 0x40000000;
    static constexpr uint32_t FLAGS_LAST_ENTRY_IN_DIR       = 0x80000000;

    uint32_t    flags;                  // Flags for this directory entry (tells type)
    uint32_t    id;                     // Unique id for the directory entry
    uint32_t    type;                   // Another 4 bytes identifying type ('*dir', '*lbl' for volume header)
    uint32_t    blockSize;              // Again should be '2048'
    uint32_t    byteCount;              // Size of the entry in bytes
    uint32_t    blockCount;             // Size of the entry in blocks
    uint32_t    burst;                  // Purpose unknown: probably something to do with low level IO control
    uint32_t    gap;                    // Purpose unknown: probably something to do with low level IO control
    char        name[MAX_NAME_LEN];     // Name of this entry
    uint32_t    lastCopyIdx;            // The number of copies of this directory entry, -1. 0 = 1 copy, 1 = 2 copies etc.
    uint32_t    copyOffsets[1];         // The offsets in blocks from the beginning of the disk to each copy (variable length). We only use the first!

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(flags);
        Endian::convertBigToHost(id);
        Endian::convertBigToHost(type);
        Endian::convertBigToHost(blockSize);
        Endian::convertBigToHost(byteCount);
        Endian::convertBigToHost(blockCount);
        Endian::convertBigToHost(burst);
        Endian::convertBigToHost(gap);
        Endian::convertBigToHost(lastCopyIdx);
        Endian::convertBigToHost(copyOffsets[0]);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Represents a pending directory that must be read
//----------------------------------------------------------------------------------------------------------------------
struct DirToRead {
    uint32_t    parentDirIdx;       // Index of the parent directory in the file system entry list
    uint32_t    firstBlockIdx;      // Index of the first block of the directory on the disk (from the disk beginning)
};

static bool areAllFlagsSet(const uint32_t flags, const uint32_t bitsToBeSet) noexcept {
    return ((flags & bitsToBeSet) == bitsToBeSet);
}

//----------------------------------------------------------------------------------------------------------------------
// Read and verify the OperaFS disc header
//----------------------------------------------------------------------------------------------------------------------
static void readAndVerifyDiscHeader(CDImageFileInputStream& cd, DiscHeader& header) THROWS {
    cd.read(header);
    header.convertBigToHostEndian();

    const bool bValidHeader = (
        (header.recordType == DiscHeader::RECORD_TYPE) &&
        (header.syncBytes[0] == DiscHeader::SYNC_BYTE) &&
        (header.syncBytes[1] == DiscHeader::SYNC_BYTE) &&
        (header.syncBytes[2] == DiscHeader::SYNC_BYTE) &&
        (header.syncBytes[3] == DiscHeader::SYNC_BYTE) &&
        (header.syncBytes[4] == DiscHeader::SYNC_BYTE) &&
        (header.version == DiscHeader::VERSION) &&
        (header.discBlockSize == OPERA_BLOCK_SIZE) &&
        (header.discNumBlocks >= 1) &&
        (header.rootDirBlockCount >= 1) &&
        (header.rootDirBlockSize == OPERA_BLOCK_SIZE) &&
        (header.rootDirCopyOffsets[0] < header.discNumBlocks)   // Make sure the first dir entry for the root is at a valid block offset!
    );

    if (!bValidHeader)
        throw BadDataException();
}

//----------------------------------------------------------------------------------------------------------------------
// Read and verfiy a directory block header
//----------------------------------------------------------------------------------------------------------------------
static void readAndVerifyDirBlockHeader(CDImageFileInputStream& cd, DirBlockHeader& header) THROWS {
    cd.read(header);
    header.convertBigToHostEndian();

    const bool bValidHeader = (
        (header.firstByte >= sizeof(DirBlockHeader)) &&
        (header.firstFreeByte >= header.firstByte)
    );

    if (!bValidHeader)
        throw BadDataException();
}

//----------------------------------------------------------------------------------------------------------------------
// Read and verfiy a directory entry
//----------------------------------------------------------------------------------------------------------------------
static void readAndVerifyDirEntry(CDImageFileInputStream& cd, DirEntry& entry) THROWS {
    cd.read(entry);
    entry.convertBigToHostEndian();

    const bool bValidEntry = (
        (entry.blockSize == OPERA_BLOCK_SIZE) &&
        (entry.byteCount <= entry.blockCount * OPERA_BLOCK_SIZE)
    );

    if (!bValidEntry)
        throw BadDataException();
}

//----------------------------------------------------------------------------------------------------------------------
// Read a single block of directory entries.
// Returns the offset of the next block of directory entries to be read after that.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t readDirEntriesBlock(
    CDImageFileInputStream& cd,
    const uint32_t parentDirIdx,
    std::queue<DirToRead>& dirsToRead,
    std::vector<FSEntry>& fsEntriesOut
) THROWS {
    // Read the block header
    DirBlockHeader dirBlockHeader;
    readAndVerifyDirBlockHeader(cd, dirBlockHeader);

    // Seek to where the data is if there is any
    const uint32_t blockDataSize = dirBlockHeader.firstFreeByte - dirBlockHeader.firstByte;

    if (blockDataSize <= 0)
        return dirBlockHeader.nextBlock;

    if (blockDataSize > OPERA_BLOCK_SIZE - (uint32_t) sizeof(DirBlockHeader))
        throw BadDataException();
    
    ASSERT(dirBlockHeader.firstByte >= sizeof(DirBlockHeader));
    cd.skip(dirBlockHeader.firstByte - sizeof(DirBlockHeader));

    // Keep track of the number of block bytes left there are.
    // Read each directory entry:
    uint32_t blockBytesLeft = blockDataSize;

    while (blockBytesLeft >= sizeof(DirEntry)) {
        // Read the directory entry
        DirEntry dirEntry;
        readAndVerifyDirEntry(cd, dirEntry);
        blockBytesLeft -= sizeof(DirEntry);

        // Skip any additional copy offsets specified
        if (dirEntry.lastCopyIdx > 0) {
            const uint32_t numBytesToSkip = dirEntry.lastCopyIdx * sizeof(uint32_t);

            if (numBytesToSkip > blockBytesLeft)
                throw BadDataException();

            cd.skip(numBytesToSkip);
            blockBytesLeft -= numBytesToSkip;
        }

        // Makeup a file system entry for this directory entry
        const bool bIsDir = areAllFlagsSet(dirEntry.flags, DirEntry::FLAGS_DIR);
        const bool bIsFile = areAllFlagsSet(dirEntry.flags, DirEntry::FLAGS_FILE);

        if (bIsDir || bIsFile) {
            // Populate the fs entry type and name fields
            FSEntry& fsEntry = fsEntriesOut.emplace_back();
            fsEntry.type = (bIsDir) ? FSEntry::TYPE_DIR : FSEntry::TYPE_FILE;

            std::memcpy(fsEntry.name, dirEntry.name, MAX_NAME_LEN);
            fsEntry.name[MAX_NAME_LEN] = 0;

            // See if we are dealing with a file or a directory.
            // If a directory then we defer filling in the directory field until later and place it in the queue.
            if (bIsDir) {
                DirToRead& dirToRead = dirsToRead.emplace();
                dirToRead.firstBlockIdx = dirEntry.copyOffsets[0];
                dirToRead.parentDirIdx = (uint32_t) fsEntriesOut.size() - 1;
            } else {
                fsEntry.file.offset = dirEntry.copyOffsets[0] * OPERA_BLOCK_SIZE;
                fsEntry.file.size = dirEntry.byteCount;
            }
        }

        // Is this the last entry in the directory?
        // If so then return that there is no more blocks ahead.
        if (areAllFlagsSet(dirEntry.flags, DirEntry::FLAGS_LAST_ENTRY_IN_DIR))
            return -1;

        // Is this the last entry in the block?
        // If so then abort reading regardless of what is ahead.
        if (areAllFlagsSet(dirEntry.flags, DirEntry::FLAGS_LAST_ENTRY_IN_BLOCK))
            break;
    }

    return dirBlockHeader.nextBlock;
}

//----------------------------------------------------------------------------------------------------------------------
// Process all the directories to be read in the given queue as well as their children.
// Saves the resulting file system entries to the given output list.
// Does not return until everything is read.
//----------------------------------------------------------------------------------------------------------------------
static void readDirEntries(
    CDImageFileInputStream& cd,
    std::queue<DirToRead>& dirsToRead,
    std::vector<FSEntry>& fsEntriesOut
) THROWS {
    while (!dirsToRead.empty()) {
        // Get this directory to read
        DirToRead dirToRead = dirsToRead.front();
        dirsToRead.pop();

        // Make a note of where in the vector the children of this parent will start.
        // N.B: DO NOT cache the reference to the parent dir as it may be invalidated when we push back into the entries list!
        {
            ASSERT(dirToRead.parentDirIdx < fsEntriesOut.size());
        
            FSEntry& parent = fsEntriesOut[dirToRead.parentDirIdx];
            ASSERT(parent.type == FSEntry::TYPE_DIR);

            parent.dir.firstChildIdx = (uint32_t) fsEntriesOut.size();
            parent.dir.numChildren = 0;
        }

        // Naviate to the first block where the directory entries are stored and read it
        const uint32_t firstBlockIdx = dirToRead.firstBlockIdx;
        cd.seek(firstBlockIdx * OPERA_BLOCK_SIZE);
        uint32_t nextBlockIdx = readDirEntriesBlock(cd, dirToRead.parentDirIdx, dirsToRead, fsEntriesOut);

        // Continue reading blocks in the directory until we are done
        while (nextBlockIdx != UINT32_MAX) {
            cd.seek((firstBlockIdx + nextBlockIdx) * OPERA_BLOCK_SIZE);
            nextBlockIdx = readDirEntriesBlock(cd, dirToRead.parentDirIdx, dirsToRead, fsEntriesOut);
        }
        
        // Now that we know it, set the number of children in the parent directory
        {
            FSEntry& parent = fsEntriesOut[dirToRead.parentDirIdx];
            const uint32_t endChildIdx = (uint32_t) fsEntriesOut.size();
            ASSERT(endChildIdx >= parent.dir.firstChildIdx);

            parent.dir.numChildren = endChildIdx - parent.dir.firstChildIdx;
        }
    }
}

bool getFSEntriesFromDiscImage(const char* const pDiscImagePath, std::vector<FSEntry>& fsEntriesOut) noexcept {
    fsEntriesOut.clear();

    try {
        // Open the requested file
        CDImageFileInputStream cd;
        cd.open(pDiscImagePath);

        // Read the disc header first
        DiscHeader discHeader;
        readAndVerifyDiscHeader(cd, discHeader);

        // Makeup the root filesystem entry
        FSEntry& rootFSEntry = fsEntriesOut.emplace_back();
        rootFSEntry.type = FSEntry::TYPE_DIR;

        // Begin reading directories
        const uint32_t initialBlockIdx = discHeader.rootDirCopyOffsets[0];

        std::queue<DirToRead> dirsToRead;
        DirToRead& dirToRead = dirsToRead.emplace();
        dirToRead.parentDirIdx = 0;
        dirToRead.firstBlockIdx = initialBlockIdx;
        
        readDirEntries(cd, dirsToRead, fsEntriesOut);

        // All good if we got to here!
        return true;
    }
    catch (...) {
        // Failed to read the filesystem!
        fsEntriesOut.clear();
        return false;
    }
}

END_NAMESPACE(OperaFS)
