#include "GameDataFS.h"

#include "Base/FileUtils.h"
#include "Config.h"
#include "ThreeDO/CDImageFileInputStream.h"
#include "ThreeDO/OperaFS.h"

BEGIN_NAMESPACE(GameDataFS)

static std::string                      gGameDataDir;       // Note: has a path separator appended to it!
static std::string                      gTempFilePath;      // Re-use for string building purposes
static std::vector<OperaFS::FSEntry>    gOperaFSEntries;

static bool isPathSeparatorChar(const char c) noexcept {
    return (c == '\\' || c == '/');
}

static constexpr char getPlatformPathSeparator() noexcept {
    #if WIN32
        return '\\';
    #else
        return '/';
    #endif
}

//----------------------------------------------------------------------------------------------------------------------
// Makeup a full path to the given file (prefixed by the game data dir) and store in the temporary file path global.
// Used to get the actual path to files to open.
//----------------------------------------------------------------------------------------------------------------------
static void makeupTempGameFilePath(const char* pRelativePath) noexcept {
    ASSERT(pRelativePath);

    gTempFilePath.clear();
    gTempFilePath.append(gGameDataDir);
    gTempFilePath.append(pRelativePath);
}

//----------------------------------------------------------------------------------------------------------------------
// Builds a list of file system entries that are contained within the CD-ROM image of 3DO Doom being used by the game.
// Will terminate with a fatal error if this process fails.
//----------------------------------------------------------------------------------------------------------------------
static void buildOperaFSEntriesList() noexcept {
    if (!OperaFS::getFSEntriesFromDiscImage(Config::gGameDataCDImagePath.c_str(), gOperaFSEntries)) {
        FATAL_ERROR_F(
            "Failed to open, read or interpret the CD-ROM image for 3DO Doom at the specified path, '%s'! "
            "Does the the file at this path exist? If so is it a valid CD-ROM image in Mode 1 / 2352 format?"
        );
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Tokenizer: returns the next component/part of a path string (i.e the bits in between the path separators).
// Moves along the given pointer until it points to something that isn't a path separator, then figures out the length
// of that path component up until thenext path separator or the end of the string.
// Returns '0' if there are no more path components in the string.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t getNextPathToken(const char*& pStr) noexcept {
    // Skip past all path separators first
    while (true) {
        const char c = pStr[0];

        if (isPathSeparatorChar(c)) {
            ++pStr;
        } else {
            if (c == 0) {
                return 0;   // At the end of the string without finding any token!
            } else {
                break;
            }
        }
    }

    // Now find where this path part ends and return the string length
    const char* pStrEnd = pStr + 1;

    while (true) {
        const char c = pStrEnd[0];

        if (isPathSeparatorChar(c) || (c == 0)) {
            break;
        } else {
            ++pStrEnd;
        }
    }

    return (uint32_t)(pStrEnd - pStr);
}

//----------------------------------------------------------------------------------------------------------------------
// Returns an opera FS entry for the given path
//----------------------------------------------------------------------------------------------------------------------
static const OperaFS::FSEntry* findOperaFSEntry(const char* const pFilePath) noexcept {
    // Note: must have initialized the list of files on the CD-ROM!
    ASSERT(!gOperaFSEntries.empty());

    // Get the first part of the path, if there is not at least 1 part then we can't get any file entry
    const char* pCurPathPart = pFilePath;
    uint32_t curPathPartLen = getNextPathToken(pCurPathPart);

    if (curPathPartLen <= 0)
        return nullptr;
    
    // Continue until we have consumed all of the string, start by searching at the root
    const OperaFS::FSEntry* pCurEntry = &gOperaFSEntries[0];

    do {
        // We've hit a file or something else, can't search in this for children
        if (pCurEntry->type != OperaFS::FSEntry::TYPE_DIR)
            return nullptr;

        // Iterate through all the children and try to find the path part we are looking for
        const uint32_t begEntryIdx = pCurEntry->dir.firstChildIdx;
        const uint32_t endEntryIdx = pCurEntry->dir.firstChildIdx + pCurEntry->dir.numChildren;

        bool bFoundCorrectChildEntry = false;

        for (uint32_t i = begEntryIdx; i < endEntryIdx; ++i) {
            const OperaFS::FSEntry& entry = gOperaFSEntries[i];

            // Check that the strings match (up until the path part length) and are the same length
            if (std::strncmp(pCurPathPart, entry.name, curPathPartLen) == 0) {
                if (entry.name[curPathPartLen] == 0) {
                    // Match - found the particular entry we are looking for!
                    bFoundCorrectChildEntry = true;
                    pCurEntry = &entry;
                    break;
                }
            }
        }

        // If we did not find what we are looking for then lookup fails
        if (!bFoundCorrectChildEntry)
            return nullptr;
        
        // Move onto the next path part
        pCurPathPart = pCurPathPart + curPathPartLen;
        curPathPartLen = getNextPathToken(pCurPathPart);

    } while (curPathPartLen > 0);

    // The returned entry MUST be a file
    if (pCurEntry->type != OperaFS::FSEntry::TYPE_FILE)
        return nullptr;
    
    return pCurEntry;
}

void init() noexcept {    
    if (Config::gbUseGameDataDirectory) {
        // Ensure that the game data dir path has a path separator at the end of it
        gGameDataDir = Config::gGameDataDirectoryPath;
        
        if (!gGameDataDir.empty()) {
            if (!isPathSeparatorChar(gGameDataDir.back())) {
                gGameDataDir.push_back(getPlatformPathSeparator());
            }
        }
    } else {
        // If we are using a CD image then we need to build a list of files on the CD
        buildOperaFSEntriesList();
    }
}

void shutdown() noexcept {
    gOperaFSEntries.clear();
    gOperaFSEntries.shrink_to_fit();
    gTempFilePath.clear();
    gTempFilePath.shrink_to_fit();
    gGameDataDir.clear();
    gGameDataDir.shrink_to_fit();
}

bool getContentsOfFile(
    const char* const pFilePath,
    std::byte*& pOutputMem,
    size_t& outputSize,
    const size_t numExtraBytes,
    const std::byte extraBytesValue
) noexcept {
    // If we are using an actual directory on disk for our game data then this is easy
    if (Config::gbUseGameDataDirectory) {
        makeupTempGameFilePath(pFilePath);
        return FileUtils::getContentsOfFile(gTempFilePath.c_str(), pOutputMem, outputSize, numExtraBytes, extraBytesValue);
    }
    
    // Initialize the output fields
    pOutputMem = nullptr;
    outputSize = 0;

    // Lookup the CD filesystem entry
    const OperaFS::FSEntry* const pFSEntry = findOperaFSEntry(pFilePath);

    if (!pFSEntry)
        return false;

    // If the entry is zero sized then this fails
    if (pFSEntry->file.size <= 0)
        return false;

    // Alloc the output buffer
    pOutputMem = new std::byte[pFSEntry->file.size + numExtraBytes];

    // Try to read the file
    try {
        CDImageFileInputStream cd;
        cd.open(Config::gGameDataCDImagePath.c_str());
        cd.seek(pFSEntry->file.offset);
        cd.readBytes(pOutputMem, pFSEntry->file.size);
    }
    catch (...) {
        // Failed to read this file - cleanup!
        delete[] pOutputMem;
        pOutputMem = nullptr;
        return false;
    }

    // If we succeeded save the output size and also set the value of the extra bytes (if requested)
    outputSize = pFSEntry->file.size;

    if (numExtraBytes) {
        std::memset(pOutputMem + pFSEntry->file.size, (int) extraBytesValue, numExtraBytes);
    }

    return true;
}

END_NAMESPACE(GameDataFS)
