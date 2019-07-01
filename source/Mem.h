#pragma once

#include <cstdint>
#include <cstdlib>

#define MEM_FREE_AND_NULL(ptr)\
    do {\
        MemFree(ptr);\
        ptr = 0;\
    } while (0)

static inline void* MemAlloc(uint32_t numBytes) {
    return malloc(numBytes);
}

static inline void MemFree(void* pMem) {
    free(pMem);
}
