#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define MEM_FREE_AND_NULL(ptr)\
    do {\
        MemFree(ptr);\
        ptr = nullptr;\
    } while (0)

static inline std::byte* MemAlloc(const uint32_t numBytes) noexcept {
    return reinterpret_cast<std::byte*>(std::malloc(numBytes));
}

static inline void MemFree(void* const pMem) noexcept {
    std::free(pMem);
}

template <class T>
static inline T& AllocObj() noexcept {
    return *reinterpret_cast<T*>(MemAlloc((uint32_t) sizeof(T)));
}

template <class T>
static inline void FreeObj(T& obj) noexcept {
    MemFree(&obj);
}
