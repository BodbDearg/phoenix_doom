#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>

//----------------------------------------------------------------------------------------------------------------------
// Utility class that allows for N bits (up to 64) of an unsigned type to be read from a stream in memory.
// The most significant bits are read first.
// The stream is merely a view/wrapper around the given memory chunk and does NOT own the memory.
//----------------------------------------------------------------------------------------------------------------------
class BitInputStream {
public:
    class StreamException {};   // Thrown when there is a problem reading etc.

    inline BitInputStream(const std::byte* const pData, const uint32_t size) noexcept
        : mpData(pData)
        , mSize(size)
        , mCurByteIdx(0)
        , mCurBitIdx(7)
    {
    }

    inline void seekToByteIndex(const uint32_t byteIndex) THROWS {
        if (mCurByteIdx > mSize) {
            throw StreamException();
        }

        mCurByteIdx = byteIndex;
        mCurBitIdx = 7;
    }

    inline uint32_t getCurByteIndex() const noexcept {
        return mCurByteIdx;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Does the actual reading of the bits.
    // Note: only unsigned integer types are supported at present!
    //------------------------------------------------------------------------------------------------------------------
    template <class OutType>
    OutType readBitsAsUInt(const uint8_t numBits) THROWS {
        static_assert(std::is_integral_v<OutType>);
        static_assert(std::is_unsigned_v<OutType>);

        ASSERT(numBits <= sizeof(OutType) * 8);
        OutType out = 0;
        uint8_t numBitsLeft = numBits;

        while (numBitsLeft > 0) {
            // Setup for reading up to 8 bits
            if (mCurByteIdx >= mSize) {
                throw StreamException();
            }

            const uint8_t curByte = (uint8_t) mpData[mCurByteIdx];
            const uint8_t numByteBitsLeft = mCurBitIdx + 1;
            const uint8_t numBitsToRead = (numBitsLeft > numByteBitsLeft) ? numByteBitsLeft : numBitsLeft;

            // Make room in the output for the new bits
            out <<= numBitsToRead;

            // Extract the required bits and add to the output
            const uint8_t shiftBitsToLSB = (mCurBitIdx + uint8_t(1) - numBitsToRead);
            const uint8_t readMask = ~(uint8_t(0xFF) << numBitsToRead);
            const uint8_t readBits = (curByte >> shiftBitsToLSB) & readMask;
            out |= readBits;

            // Move onto the next byte if appropriate and count the bits read
            if (numBitsToRead >= numByteBitsLeft) {
                ++mCurByteIdx;
                mCurBitIdx = 7;
            }
            else {
                mCurBitIdx -= numBitsToRead;
            }

            numBitsLeft -= numBitsToRead;
        }

        return out;
    }

    //------------------------------------------------------------------------------------------------------------------
    // Aligns the current stream pointer to the start of the next 64-bit boundary.
    //------------------------------------------------------------------------------------------------------------------
    void align64() THROWS {
        uint32_t newCurByteIdx = mCurByteIdx;

        if (mCurBitIdx < 7) {
            ++newCurByteIdx;
        }

        newCurByteIdx = (newCurByteIdx + uint32_t(7)) & (~uint32_t(7));

        if (newCurByteIdx > mSize) {
            throw StreamException();
        }

        mCurByteIdx = newCurByteIdx;
        mCurBitIdx = 7;
    }

private:
    const std::byte* const  mpData;
    uint32_t                mSize;
    uint32_t                mCurByteIdx;
    uint8_t                 mCurBitIdx;
};
