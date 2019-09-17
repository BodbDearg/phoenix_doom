#pragma once

#include <cstddef>
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Represents a single resource in the manager
//----------------------------------------------------------------------------------------------------------------------
struct Resource {
    uint32_t    number;
    uint32_t    type;
    uint32_t    offset;     // Offset within the resource file
    uint32_t    size;       // Size of the resource
    std::byte*  pData;      // Non null if the resource is loaded
};
