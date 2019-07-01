#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define MEM_FREE_AND_NULL(ptr)\
    do {\
        MemFree(ptr);\
        ptr = 0;\
    } while (0)

static inline std::byte* MemAlloc(uint32_t numBytes) {
    return (std::byte*) malloc(numBytes);
}

static inline void MemFree(void* pMem) {
    free(pMem);
}
