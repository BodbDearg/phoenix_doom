#include "CelUtils.h"

#include "Base/Endian.h"
#include "Base/Macros.h"
#include "Base/Mem.h"
#include <cstring>
#include <type_traits>

BEGIN_NAMESPACE(CelUtils)

//-------------------------------------------------------------------------------------------------
// Utility class that allows for N bits (up to 64) to be read from a stream of unknown size.
// The most significant bits are read first.
//-------------------------------------------------------------------------------------------------
class BitStream {
public:
    inline BitStream(const std::byte* const pData, const uint32_t size) noexcept
        : mpData(pData)
        , mSize(size)
        , mCurByteIdx(0)
        , mCurBitIdx(7)
    {
    }

    inline void seekToByteIndex(const uint32_t byteIndex) noexcept {
        mCurByteIdx = byteIndex;
        mCurBitIdx = 7;
    }

    inline uint32_t getCurByteIndex() const noexcept {
        return mCurByteIdx;
    }

    // Does the actual reading of the bits.
    // Note: only unsigned integer types are supported at present!
    template <class OutType>
    OutType readBitsAsUInt(const uint8_t numBits) noexcept {
        static_assert(std::is_integral_v<OutType>);
        static_assert(std::is_unsigned_v<OutType>);

        ASSERT(numBits <= sizeof(OutType) * 8);
        OutType out = 0;
        uint8_t numBitsLeft = numBits;

        while (numBitsLeft > 0) {
            // Setup for reading up to 8 bits
            ASSERT(mCurByteIdx < mSize);
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

    // Aligns the current stream pointer to the start of the next uint64_t
    void u64Align() noexcept {
        if (mCurBitIdx < 7) {
            mCurBitIdx = 7;
            ++mCurByteIdx;
        }

        mCurByteIdx = (mCurByteIdx + uint32_t(7)) & ~uint32_t(7);
    }

private:
    const std::byte* const  mpData;
    uint32_t                mSize;
    uint32_t                mCurByteIdx;
    uint8_t                 mCurBitIdx;
};

//-------------------------------------------------------------------------------------------------
// Pack modes for rows of packed CEL image data
//-------------------------------------------------------------------------------------------------
enum class CelPackMode : uint8_t {
    END = 0,            // End of the packed data, no pixels follow
    LITERAL = 1,        // A number of verbatim pixels follow
    TRANSPARENT = 2,    // A number of transparent pixels follow
    REPEAT = 3          // A pixel follows, which is then repeated a number of times
};

//-------------------------------------------------------------------------------------------------
// Color formats for CEL image data
//-------------------------------------------------------------------------------------------------
enum class CelColorMode : uint8_t {
    BPP_1,
    BPP_2,
    BPP_4,
    BPP_6,
    BPP_8,
    BPP_16
};

//-------------------------------------------------------------------------------------------------
// Useful constants
//-------------------------------------------------------------------------------------------------

static constexpr uint32_t CCB_FLAG_PACKED = 0x00000200;     // The CCB flag set when the CEL image data is in packed format
static constexpr uint32_t CCB_FLAG_LINEAR = 0x00000010;     // The CCB flag set when the CEL image data is NOT color indexed

// Bitwise OR this with the decoded color to ensure an opaque pixel
static constexpr uint16_t OPAQUE_PIXEL_BITS = 0x8000;

//-------------------------------------------------------------------------------------------------
// Determines how many bits per pixel a CEL is
//-------------------------------------------------------------------------------------------------
static uint8_t getCCBBitsPerPixel(const CelControlBlock& ccb) noexcept {
    const uint32_t ccbPre0 = byteSwappedU32(ccb.pre0);
    const uint8_t mode = ccbPre0 & 0x07;                    // The first 3-bits define the format

    switch (mode) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        case 4: return 6;
        case 5: return 8;
        case 6: return 16;
    }

    return 0;
}

//-------------------------------------------------------------------------------------------------
// Decodes unpacked (raw) CEL image data.
// The pointer to the image data must point to the start data for the first row.
//-------------------------------------------------------------------------------------------------
static void decodeUnpackedCelImageData(
    const std::byte* const pImageData,
    const uint32_t imageDataSize,
    const uint16_t* const pPLUT,
    const uint16_t imageW,
    const uint16_t imageH,
    const uint8_t imageBPP,
    const bool bColorIndexed,
    uint16_t* const pImageOut
) noexcept {
    // Setup for reading
    BitStream bitStream(pImageData, imageDataSize);
    uint16_t* pCurOutputPixel = pImageOut;

    // Read the entire image
    if (bColorIndexed) {
        for (uint16_t y = 0; y < imageH; ++y) {
            for (uint16_t x = 0; x < imageW; ++x) {
                const uint8_t colorIdx = bitStream.readBitsAsUInt<uint8_t>(imageBPP);
                const uint16_t color = byteSwappedU16(pPLUT[colorIdx]) | OPAQUE_PIXEL_BITS;     // Note: making 'always opaque' only works assuming the image is used as a 'masked' image...
                *pCurOutputPixel = color;
                ++pCurOutputPixel;
            }

            bitStream.u64Align();
        }
    } else {
        ASSERT(imageBPP == 16);

        for (uint16_t y = 0; y < imageH; ++y) {
            for (uint16_t x = 0; x < imageW; ++x) {
                const uint16_t color = bitStream.readBitsAsUInt<uint16_t>(16);
                *pCurOutputPixel = color;
                ++pCurOutputPixel;
            }

            bitStream.u64Align();
        }
    }
}

//-------------------------------------------------------------------------------------------------
// Decodes the actual CEL image data.
// The pointer to the image data must point to the start data for the first row.
//-------------------------------------------------------------------------------------------------
static void decodePackedCelImageData(
    const std::byte* const pImageData,
    const uint32_t imageDataSize,
    const uint16_t* const pPLUT,
    const uint16_t imageW,
    const uint16_t imageH,
    const uint8_t imageBPP,
    const bool bColorIndexed,
    uint16_t* const pImageOut
) noexcept {
    ASSERT(bColorIndexed || imageBPP == 16);

    // Alloc output image and start decoding each row
    const std::byte* pCurRowData = pImageData;
    const std::byte* pNextRowData = nullptr;

    for (uint16_t y = 0; y < imageH; ++y) {
        // Firstly read offset to the next row and store the pointer.
        // That is the first bit of info for each row of pixels.
        const uint32_t imageDataSizeLeft = imageDataSize - (uint32_t)(pCurRowData - pImageData);
        BitStream bitStream(pCurRowData, imageDataSizeLeft);
        uint32_t nextRowOffset;

        {
            // For 8 and 16-bit CEL images the offset is encoded in 10-bits of a u16.
            // For other CEL image formats just a single byte is used.
            if (imageBPP >= 8) {
                nextRowOffset = bitStream.readBitsAsUInt<uint16_t>(16) & uint16_t(0x3FF);
            }
            else {
                nextRowOffset = bitStream.readBitsAsUInt<uint16_t>(8);
            }

            // Both 3DO Doom and the GIMP CEL plugin do this to calculate the final offset!
            // I don't know WHY this is, just the way things are? (shrugs)
            nextRowOffset += 2;
            nextRowOffset *= 4;
            pNextRowData = pCurRowData + nextRowOffset;
        }

        const uint32_t rowSize = nextRowOffset;

        // Decode this row
        uint16_t* const pRowPixels = pImageOut + y * imageW;
        uint16_t x = 0;

        do {
            // Determine the pack mode for this pixel packet.
            // If the row is ended then fill any remaining pixels in as blank:
            const CelPackMode packMode = (CelPackMode) bitStream.readBitsAsUInt<uint8_t>(2);

            if (packMode == CelPackMode::END) {
                std::memset(pRowPixels + x, 0, (imageW - x) * sizeof(uint16_t));
                break;
            }

            // Otherwise read the number of pixels in this packet and decide what to do.
            // Note that the lowest count possible is '1', hence it is added implicitly:
            const uint16_t packCount = bitStream.readBitsAsUInt<uint16_t>(6) + 1;
            ASSERT(x + packCount <= imageW);

            if (packMode == CelPackMode::LITERAL) {
                // A number of literal pixels follow.
                // Read each pixel and output it to the output buffer:
                const uint16_t endX = x + packCount;

                while (x < endX) {
                    uint16_t color;

                    if (bColorIndexed) {
                        const uint16_t colorIdx = bitStream.readBitsAsUInt<uint16_t>(imageBPP);
                        color = byteSwappedU16(pPLUT[colorIdx]) | OPAQUE_PIXEL_BITS;
                    }
                    else {
                        color = bitStream.readBitsAsUInt<uint16_t>(16) | OPAQUE_PIXEL_BITS;
                    }

                    pRowPixels[x] = color;
                    ++x;
                }
            }
            else if (packMode == CelPackMode::TRANSPARENT) {
                // A number of transparent pixels follow, write all 0s to the output buffer:
                std::memset(pRowPixels + x, 0, packCount * sizeof(uint16_t));
                x += packCount;
            }
            else {
                // A repeated pixel follows
                ASSERT(packMode == CelPackMode::REPEAT);
                uint16_t color;

                if (bColorIndexed) {
                    const uint16_t colorIdx = bitStream.readBitsAsUInt<uint16_t>(imageBPP);
                    color = byteSwappedU16(pPLUT[colorIdx]) | OPAQUE_PIXEL_BITS;
                }
                else {
                    color = bitStream.readBitsAsUInt<uint16_t>(16) | OPAQUE_PIXEL_BITS;
                }

                const uint16_t endX = x + packCount;

                while (x < endX) {
                    pRowPixels[x] = color;
                    ++x;
                }
            }
        } while (bitStream.getCurByteIndex() < rowSize && x < imageW);

        // Move onto the next row
        pCurRowData = pNextRowData;
    }
}

//-------------------------------------------------------------------------------------------------
// Decodes CEL image data.
// The pointer to the image data must point to the start data for the first row.
//-------------------------------------------------------------------------------------------------
static uint16_t* decodeCelImageData(
    const std::byte* const pImageData,
    const uint32_t imageDataSize,
    const uint16_t* const pPLUT,
    const uint16_t imageW,
    const uint16_t imageH,
    const uint8_t imageBPP,
    const bool bImageIsPacked,
    const bool bColorIndexed
) noexcept {
    uint16_t* const pImageOut = (uint16_t*) MemAlloc(imageW * imageH * sizeof(uint16_t));

    if (!bColorIndexed && imageBPP != 16) {
        FATAL_ERROR("Images that are not color indexed MUST be 16 bpp!");
    }

    if (bImageIsPacked) {
        decodePackedCelImageData(
            pImageData,
            imageDataSize,
            pPLUT,
            imageW,
            imageH,
            imageBPP,
            bColorIndexed,
            pImageOut
        );
    } else {
        decodeUnpackedCelImageData(
            pImageData,
            imageDataSize,
            pPLUT,
            imageW,
            imageH,
            imageBPP,
            bColorIndexed,
            pImageOut
        );
    }

    return pImageOut;
}

uint16_t getCCBWidth(const CelControlBlock* const pCCB) noexcept {
    ASSERT(pCCB);

    // DC: this logic comes from the original 3DO Doom source.
    // It can be found in the burgerlib function 'GetShapeWidth()':
    uint32_t width = byteSwappedU32(pCCB->pre1) & 0x7FF;                // Get the HCount bits
    width += 1;                                                         // Needed to get the TRUE result
    return width;
}

uint16_t getCCBHeight(const CelControlBlock* const pCCB) noexcept {
    ASSERT(pCCB);

    // DC: this logic comes from the original 3DO Doom source.
    // It be found in the burgerlib function 'GetShapeHeight()':
    uint32_t height = byteSwappedU32(pCCB->pre0) >> 6;                  // Get the VCount bits
    height &= 0x3FF;                                                    // Mask off unused bits
    height += 1;                                                        // Needed to get the TRUE result
    return height;
}

void decodeDoomCelSprite(
    const CelControlBlock* const pCCB,
    const uint32_t celDataSize,
    uint16_t** pImageOut,
    uint16_t* pImageWidthOut,
    uint16_t* pImageHeightOut
) noexcept {
    ASSERT(pCCB);
    ASSERT(pImageOut);

    // Get image width and height
    const uint16_t imageW = getCCBWidth(pCCB);
    const uint16_t imageH = getCCBHeight(pCCB);

    if (pImageWidthOut) {
        *pImageWidthOut = imageW;
    }

    if (pImageHeightOut) {
        *pImageHeightOut = imageH;
    }

    ASSERT(imageW > 0);
    ASSERT(imageH > 0);

    // Grabbing various bits of CCB info and endian correcting
    const uint32_t imageCCBFlags = byteSwappedU32(pCCB->flags);
    const uint32_t imageDataOffset = byteSwappedU32(pCCB->sourcePtr) + 12;  // For some reason 3DO Doom added '12' to this offset to get the image data?
    const uint8_t imageBPP = getCCBBitsPerPixel(*pCCB);

    if (imageBPP > 16) {
        FATAL_ERROR("Images with a bit depth greater than 16 bpp are NOT supported!");
    }

    // Get the pointers to the PLUT and image data
    const std::byte* const pCelBytes = (const std::byte*) pCCB;
    const uint16_t* const pPLUT = (const uint16_t*)(pCelBytes + 60);        // 3DO Doom harcoded this offset?
    const std::byte* const pImageData = pCelBytes + imageDataOffset;

    ASSERT(pImageData < pCelBytes + celDataSize);
    const uint32_t imageDataSize = celDataSize - (uint32_t)(pImageData - pCelBytes);

    // Determining whether the CCB is color indexed or not SHOULD be a simple case of checking the 'linear' flag but
    // sometimes this lies to us for some of the Doom resources.
    // Fix up some contradictions here...
    bool bIsColorIndexed = ((imageCCBFlags & CCB_FLAG_LINEAR) == 0);

    if (imageBPP < 8) {
        bIsColorIndexed = true;     // MUST be color indexed
    }
    else if (imageBPP >= 16) {
        bIsColorIndexed = false;    // MUST NOT be color indexed
    }

    // Decode and return!
    *pImageOut = decodeCelImageData(
        pImageData,
        imageDataSize,
        pPLUT,
        imageW,
        imageH,
        imageBPP,
        ((imageCCBFlags & CCB_FLAG_PACKED) != 0),
        bIsColorIndexed
    );
}

END_NAMESPACE(CelUtils)
