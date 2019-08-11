#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define MEM_FREE_AND_NULL(ptr)\
    do {\
        MemFree(ptr);\
        ptr = 0;\
    } while (0)

static inline std::byte* MemAlloc(uint32_t numBytes) noexcept {
    return (std::byte*) malloc(numBytes);
}

static inline void MemFree(void* pMem) noexcept {
    free(pMem);
}
