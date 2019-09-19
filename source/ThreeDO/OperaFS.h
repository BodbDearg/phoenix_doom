#pragma once

#include "Base/Macros.h"
#include <cstdint>
#include <vector>

//----------------------------------------------------------------------------------------------------------------------
// Utility functions and data structures relating to the Opera filesystem used by 3DO CD-ROMS
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(OperaFS)

// Maximum length of a file or directory name in OperaFS
static constexpr uint32_t MAX_NAME_LEN = 32;

//----------------------------------------------------------------------------------------------------------------------
// Represents a single entry in the file system
//----------------------------------------------------------------------------------------------------------------------
struct FSEntry {
    // Fields for when the entry represents a directory
    struct DirFields {
        uint32_t firstChildIdx;     // Index of the first child filesystem entry
        uint32_t numChildren;       // The number of child filesystem entries of the directory
    };

    // Fields for when the entry represents a file
    struct FileFields {
        uint32_t offset;    // Offset of the file data
        uint32_t size;      // Size of the file data
    };

    // Values for filesystem entry type
    static constexpr uint8_t TYPE_FILE = 0;
    static constexpr uint8_t TYPE_DIR = 1;

    uint8_t type;                       // What type of entry this is
    char    name[MAX_NAME_LEN + 1];     // Note: I null terminate this so adding an extra byte!
    uint8_t _unused[2];                 // Unused padding bytes

    union {
        DirFields   dir;
        FileFields  file;
    };
};

// Builds a complete list of filesystem entries for the given 3DO disc image path.
// The root entry is always first in the list; returns false on failure.
bool getFSEntriesFromDiscImage(const char* const pDiscImagePath, std::vector<FSEntry>& fsEntriesOut) noexcept;

END_NAMESPACE(OperaFS)
