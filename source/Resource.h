#pragma once

#include <stdint.h>

//---------------------------------------------------------------------------------------------------------------------
// Represents a single resource in the manager
//---------------------------------------------------------------------------------------------------------------------
typedef struct Resource {
    uint32_t    number;
    uint32_t    type;
    uint32_t    offset;     // Offset within the resource file
    uint32_t    size;       // Size of the resource
    void*       pData;      // Non null if the resource is loaded
} Resource;
