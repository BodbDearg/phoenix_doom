#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define MEM_FREE_AND_NULL(ptr)\
    do {\
        MemFree(ptr);\
        ptr = nullptr;\
    } while (0)

inline std::byte* MemAlloc(const uint32_t numBytes) noexcept {
    return reinterpret_cast<std::byte*>(std::malloc(numBytes));
}

inline void MemFree(void* const pMem) noexcept {
    std::free(pMem);
}

template <class T>
inline T& AllocObj() noexcept {
    return *reinterpret_cast<T*>(MemAlloc((uint32_t) sizeof(T)));
}

template <class T>
inline void FreeObj(T& obj) noexcept {
    MemFree(&obj);
}

template <class T>
inline void MemClear(T& obj) noexcept {
    std::memset((void*) &obj, 0, sizeof(obj));
}
