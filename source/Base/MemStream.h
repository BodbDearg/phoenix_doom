#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

//----------------------------------------------------------------------------------------------------------------------
// Exception thrown when there are not enough bytes to read in a memory stream
//----------------------------------------------------------------------------------------------------------------------
class MemStreamException {
};

//----------------------------------------------------------------------------------------------------------------------
// Allows reading from a stream in memory
//----------------------------------------------------------------------------------------------------------------------
class MemStream {
public:
    inline MemStream(const std::byte* const pData, const uint32_t size) noexcept
        : mpData(pData)
        , mSize(size)
        , mCurByteIdx(0)
    {
    }

    inline MemStream(const MemStream& other) noexcept = default;

    inline uint32_t tell() const noexcept {
        return mCurByteIdx;
    }

    inline const std::byte* getCurData() const noexcept {
        return mpData + mCurByteIdx;
    }

    inline uint32_t getNumBytesLeft() const noexcept {
        return mSize - mCurByteIdx;
    }

    inline bool hasBytesLeft(const uint32_t numBytes = 1) const noexcept {
        return (getNumBytesLeft() >= numBytes);
    }

    inline void ensureBytesLeft(const uint32_t numBytes) THROWS {
        if (!hasBytesLeft(numBytes)) {
            throw MemStreamException();
        }
    }

    inline void consume(const uint32_t numBytes) THROWS {
        ensureBytesLeft(numBytes);
        mCurByteIdx += numBytes;
    }

    template <class T>
    inline void read(T& output) THROWS {
        ensureBytesLeft(sizeof(T));
        std::memcpy(&output, mpData + mCurByteIdx, sizeof(T));
        mCurByteIdx += sizeof(T);
    }

    template <class T>
    inline T read() THROWS {
        ensureBytesLeft(sizeof(T));

        T output;
        std::memcpy(&output, mpData + mCurByteIdx, sizeof(T));
        mCurByteIdx += sizeof(T);

        return output;
    }

    inline void readBytes(std::byte* const pDst, const uint32_t numBytes) THROWS {
        ensureBytesLeft(numBytes);
        std::memcpy(pDst, mpData + mCurByteIdx, numBytes);
        mCurByteIdx += numBytes;
    }

    // Aligns the memory stream to the given byte boundary (2, 4, 8 etc.)
    // Note: call is ignored if at the end of the stream!
    inline void align(const uint32_t numBytes) THROWS {
        if (mCurByteIdx >= mSize)
            return;

        if (numBytes >= 2) {
            const uint32_t modulus = mCurByteIdx % numBytes;

            if (modulus > 0) {
                const uint32_t numBytesToAlign = numBytes - modulus;
                ensureBytesLeft(numBytesToAlign);
                mCurByteIdx += numBytesToAlign;
            }
        }
    }

private:
    const std::byte* const  mpData;
    const uint32_t          mSize;
    uint32_t                mCurByteIdx;
};
