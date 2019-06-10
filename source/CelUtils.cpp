#include "CelUtils.h"

#include "Endian.h"
#include "Macros.h"
#include "Mem.h"
#include <cstddef>
#include <cstdio>
#include <cstring>

// Pack modes for rows of packed CEL image data
enum class CelPackMode : uint8_t {
    END = 0,            // End of the packed data, no pixels follow
    LITERAL = 1,        // A number of verbatim pixels follow
    TRANSPARENT = 2,    // A number of transparent pixels follow
    REPEAT = 3          // A pixel follows, which is then repeated a number of times
};

// Color formats for CEL image data
enum class CelColorMode : uint8_t {
    BPP_1,
    BPP_2,
    BPP_4,
    BPP_6,
    BPP_8,
    BPP_16
};

// The CCB flag set when CEL image data is in packed format
static constexpr uint32_t CCB_FLAG_PACKED = 0x00000200;

// Determines how many bits per pixel a CEL is
static uint8_t getCCBBitsPerPixel(const CelControlBlock& ccb) {
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

// Decodes the control byte for a packed of packed pixels
static void decodePackedCelControlByte(const uint8_t controlByte, CelPackMode& packMode, uint8_t& packedCount) noexcept {
    packMode = (CelPackMode) (controlByte & uint8_t(0x03));

    if (packMode == CelPackMode::END) {
        packedCount = 0;
    }
    else {
        packedCount = ((controlByte >> 3) & uint8_t(0x1F)) + uint8_t(1);
    }
}

extern "C" {

uint16_t getCCBWidth(const CelControlBlock* const pCCB) {
    ASSERT(pCCB);

    // DC: this logic comes from the original 3DO Doom source.
    // It can be found in the burgerlib function 'GetShapeWidth()':
    uint32_t width = byteSwappedU32(pCCB->pre1) & 0x7FF;                // Get the HCount bits
    width += 1;                                                         // Needed to get the TRUE result
    return width;
}

uint16_t getCCBHeight(const CelControlBlock* const pCCB) {
    ASSERT(pCCB);

    // DC: this logic comes from the original 3DO Doom source.
    // It be found in the burgerlib function 'GetShapeHeight()':
    uint32_t height = byteSwappedU32(pCCB->pre0) >> 6;                  // Get the VCount bits
    height &= 0x3FF;                                                    // Mask off unused bits
    height += 1;                                                        // Needed to get the TRUE result
    return height;
}

// TODO: TEMP REFERENCE:
// https://github.com/libretro/4do-libretro/blob/master/libfreedo/freedo_madam.c

void decodeDoomCelSprite(
    const CelControlBlock* const pCCB,
    uint16_t** pImageOut,
    uint16_t* pImageWidthOut,
    uint16_t* pImageHeightOut
) {
    ASSERT(pCCB);
    ASSERT(pImageOut);

    // Get image width and height
    const uint16_t imgW = getCCBWidth(pCCB);
    const uint16_t imgH = getCCBHeight(pCCB);

    if (pImageWidthOut) {
        *pImageWidthOut = imgW;
    }

    if (pImageHeightOut) {
        *pImageHeightOut = imgH;
    }

    // Grabbing various bits of CCB info and endian correcting
    const uint32_t ccbFlags = byteSwappedU32(pCCB->flags);

    if (ccbFlags & CCB_FLAG_PACKED != 0) {
        FATAL_ERROR("Unable to decode non packed cels at the moment!");
    }

    const uint32_t imageDataOffset = byteSwappedU32(pCCB->sourcePtr) + 12;  // For some reason 3DO Doom added '12' to this offset to get the image data?    
    const uint8_t bpp = getCCBBitsPerPixel(*pCCB);

    // Alloc output data
    uint16_t* const pDecodedImage = (uint16_t*) MemAlloc(imgW * imgH * sizeof(uint16_t));
    *pImageOut = pDecodedImage;

    // Setup basic pointers
    const std::byte* const pCelBytes = (const std::byte*) pCCB;
    const uint16_t* const pPLUT = (const uint16_t*)(pCelBytes + 64);        // 3DO Doom harcoded this offset?
    const std::byte* const pImageData = pCelBytes + imageDataOffset;

    // Start decoding each row
    const uint8_t* pCurByte = (const uint8_t*) pImageData;
    
    for (uint16_t y = 0; y < imgH; ++y) {
        // Get the offset to the next row.
        // This logic seems strange and not at all what I would expect of the 3DO format but it's what 3DO Doom did so...?
        const uint16_t nextRowOffset = (((const uint8_t*) pCurByte)[0] + 2) * 4;
        const uint8_t* pNextRowBytes = pCurByte + nextRowOffset;

        pCurByte += 2;
        
        // Decode the row
        uint16_t* const pOutputRow = pDecodedImage + (y * imgW);
        uint16_t x = 0;

        while (x < imgW) {
            // Read the control byte for the pixel packet
            const uint8_t controlByte = (uint8_t) *pCurByte;
            ++pCurByte;

            CelPackMode packMode = {};
            uint8_t packedCount = 0;
            decodePackedCelControlByte(controlByte, packMode, packedCount);

            // See what the packet type is
            if (packMode == CelPackMode::END) {
                // End of line, treat any remaining row pixels as whitespace
                if (x < imgW) {
                    std::memset(pOutputRow + x, 0x00, (imgW - x) * sizeof(uint16_t));
                    break;
                }
            }
            else if (packMode == CelPackMode::LITERAL) {
                // A packet of literally stored pixels
                ASSERT(x + packedCount <= imgW);

                for (uint8_t pixelNum = 0; pixelNum < packedCount; ++pixelNum) {
                    // Grab the pixel
                    const uint8_t colorIdx = (uint8_t) *pCurByte;
                    const uint16_t color = byteSwappedU16(pPLUT[colorIdx]);
                    ++pCurByte;
                    
                    // Save the pixel
                    uint16_t* const pOutputPixel = pOutputRow + x;
                    *pOutputPixel = color;
                    ++x;
                }
            }
            else if (packMode == CelPackMode::TRANSPARENT) {
                // A packet of transparent pixels
                ASSERT(x + packedCount <= imgW);
                std::memset(pOutputRow + x, 0x00, packedCount * sizeof(uint16_t));
                x += packedCount;
            }
            else {
                // A packet of repeated pixels
                ASSERT(packMode == CelPackMode::REPEAT);
                ASSERT(x + packedCount <= imgW);

                // Grab the pixel
                const uint8_t colorIdx = (uint8_t) *pCurByte;
                const uint16_t color = byteSwappedU16(pPLUT[colorIdx]);
                ++pCurByte;

                // Repeat it a number of times
                for (uint8_t pixelNum = 0; pixelNum < packedCount; ++pixelNum) {
                    uint16_t* const pOutputPixel = pOutputRow + x;
                    *pOutputPixel = color;
                    ++x;
                }
            }
        }

        // Move onto the next row
        pCurByte = pNextRowBytes;
    }
}

}
