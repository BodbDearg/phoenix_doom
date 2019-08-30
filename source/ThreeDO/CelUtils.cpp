#include "CelUtils.h"

#include "Base/BitStream.h"
#include "Base/Endian.h"
#include "Base/Macros.h"
#include "Base/Mem.h"
#include <cstring>
#include <type_traits>

BEGIN_NAMESPACE(CelUtils)

//----------------------------------------------------------------------------------------------------------------------
// Exception thrown when there is an error decoding a CEL image
//----------------------------------------------------------------------------------------------------------------------
class CelDecodeException {};

//----------------------------------------------------------------------------------------------------------------------
// Pack modes for rows of packed CEL image data
//----------------------------------------------------------------------------------------------------------------------
enum class CelPackMode : uint8_t {
    END = 0,            // End of the packed data, no pixels follow
    LITERAL = 1,        // A number of verbatim pixels follow
    TRANSPARENT = 2,    // A number of transparent pixels follow
    REPEAT = 3          // A pixel follows, which is then repeated a number of times
};

//----------------------------------------------------------------------------------------------------------------------
// Color formats for CEL image data
//----------------------------------------------------------------------------------------------------------------------
enum class CelColorMode : uint8_t {
    BPP_1,
    BPP_2,
    BPP_4,
    BPP_6,
    BPP_8,
    BPP_16
};

//----------------------------------------------------------------------------------------------------------------------
// Useful constants
//----------------------------------------------------------------------------------------------------------------------

static constexpr uint32_t CCB_FLAG_PACKED = 0x00000200;     // The CCB flag set when the CEL image data is in packed format
static constexpr uint32_t CCB_FLAG_LINEAR = 0x00000010;     // The CCB flag set when the CEL image data is NOT color indexed

// Bitwise OR this with the decoded color to ensure an opaque pixel
static constexpr uint16_t OPAQUE_PIXEL_BITS = 0x8000;

//----------------------------------------------------------------------------------------------------------------------
// Transforms a cel image that uses a special color to represent transparency to a regular ARGB1555 image with alpha.
// This simplifies & unifies blitting operations elsewhere if the color format for cel images is the same as other
// assets (Doom sprites etc.) in the game.
//----------------------------------------------------------------------------------------------------------------------
static void transformMaskedImageToImageWithAlpha(CelImage& image) noexcept {
    constexpr uint16_t TRANS_MASK = 0x7FFFu;
    constexpr uint16_t OPAQUE_BIT = 0x8000u;

    const uint32_t numPixels = image.width * image.height;
    uint16_t* pCurPixel = image.pPixels;

    // Do as many 8 pixel batches as we can at once
    uint16_t* const pEndPixel8 = image.pPixels + (numPixels & 0xFFFFFFF8);
    uint16_t* const pEndPixel = image.pPixels + image.width * image.height;

    while (pCurPixel < pEndPixel8) {
        const uint16_t pixel1 = pCurPixel[0];
        const uint16_t pixel2 = pCurPixel[1];
        const uint16_t pixel3 = pCurPixel[2];
        const uint16_t pixel4 = pCurPixel[3];
        const uint16_t pixel5 = pCurPixel[4];
        const uint16_t pixel6 = pCurPixel[5];
        const uint16_t pixel7 = pCurPixel[6];
        const uint16_t pixel8 = pCurPixel[7];

        const bool bIsTransparent1 = ((pixel1 & TRANS_MASK) == 0);
        const bool bIsTransparent2 = ((pixel2 & TRANS_MASK) == 0);
        const bool bIsTransparent3 = ((pixel3 & TRANS_MASK) == 0);
        const bool bIsTransparent4 = ((pixel4 & TRANS_MASK) == 0);
        const bool bIsTransparent5 = ((pixel5 & TRANS_MASK) == 0);
        const bool bIsTransparent6 = ((pixel6 & TRANS_MASK) == 0);
        const bool bIsTransparent7 = ((pixel7 & TRANS_MASK) == 0);
        const bool bIsTransparent8 = ((pixel8 & TRANS_MASK) == 0);

        pCurPixel[0] = (bIsTransparent1) ? 0 : pixel1 | OPAQUE_BIT;
        pCurPixel[1] = (bIsTransparent2) ? 0 : pixel2 | OPAQUE_BIT;
        pCurPixel[2] = (bIsTransparent3) ? 0 : pixel3 | OPAQUE_BIT;
        pCurPixel[3] = (bIsTransparent4) ? 0 : pixel4 | OPAQUE_BIT;
        pCurPixel[4] = (bIsTransparent5) ? 0 : pixel5 | OPAQUE_BIT;
        pCurPixel[5] = (bIsTransparent6) ? 0 : pixel6 | OPAQUE_BIT;
        pCurPixel[6] = (bIsTransparent7) ? 0 : pixel7 | OPAQUE_BIT;
        pCurPixel[7] = (bIsTransparent8) ? 0 : pixel8 | OPAQUE_BIT;

        pCurPixel += 8;
    }

    // Transform any remaining pixels that don't fit into a batch
    while (pCurPixel < pEndPixel) {
        const uint16_t pixel = pCurPixel[0];
        const bool bIsTransparent = ((pixel & TRANS_MASK) == 0);
        pCurPixel[0] = (bIsTransparent) ? 0 : pixel | OPAQUE_BIT;
        ++pCurPixel;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Determines how many bits per pixel a CEL is
//----------------------------------------------------------------------------------------------------------------------
static uint8_t getCCBBitsPerPixel(const CelControlBlock& ccb) noexcept {
    const uint32_t ccbPre0 = Endian::bigToHost(ccb.pre0);
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

//----------------------------------------------------------------------------------------------------------------------
// Decodes unpacked (raw) CEL image data.
// The pointer to the image data must point to the start data for the first row.
//----------------------------------------------------------------------------------------------------------------------
static void decodeUnpackedCelPixelData(
    const std::byte* const pImageData,
    const uint32_t imageDataSize,
    const uint16_t* const pPLUT,
    const uint16_t imageW,
    const uint16_t imageH,
    const uint8_t imageBPP,
    const bool bColorIndexed,
    uint16_t* const pImageOut
) THROWS {
    // Setup for reading
    BitStream bitStream(pImageData, imageDataSize);
    uint16_t* pCurOutputPixel = pImageOut;

    // Figure out the size of each row after 64-bit alignment is applied; use that to decide whether to do 64-bit
    // alignment or not. For some reason most CELs in the game require 64-bit alignment at the end of each row but
    // there are a couple of odd CELS (like the loading plaque) that CAN'T have this alignment applied without
    // overstepping the boundary of the data. Not sure of a proper way to determine this, maybe some mysterious CCB flag?
    // This works just as well anyway...
    bool bDo64BitAlignment = true;

    {
        const uint32_t rowSizeInBits = imageBPP * imageW;
        const uint32_t alignedRowSizeInBits = (rowSizeInBits + 63) & (~63u);
        const uint32_t alignedRowSizeInBytes = alignedRowSizeInBits / 8;
        const uint32_t totalImageSizeWithAlignment = alignedRowSizeInBytes * imageH;

        if (totalImageSizeWithAlignment > imageDataSize) {
            bDo64BitAlignment = false;
        }
    }

    // Read the entire image
    if (bColorIndexed) {
        for (uint16_t y = 0; y < imageH; ++y) {
            if (bDo64BitAlignment) {
                bitStream.align64();
            }

            for (uint16_t x = 0; x < imageW; ++x) {
                const uint8_t colorIdx = bitStream.readBitsAsUInt<uint8_t>(imageBPP);
                const uint16_t color = Endian::bigToHost(pPLUT[colorIdx]) | OPAQUE_PIXEL_BITS;  // Note: making 'always opaque' only works assuming the image is used as a 'masked' image...
                *pCurOutputPixel = color;
                ++pCurOutputPixel;
            }
        }
    } else {
        ASSERT(imageBPP == 16);

        for (uint16_t y = 0; y < imageH; ++y) {
            if (bDo64BitAlignment) {
                bitStream.align64();
            }

            for (uint16_t x = 0; x < imageW; ++x) {
                const uint16_t color = bitStream.readBitsAsUInt<uint16_t>(16);
                *pCurOutputPixel = color;
                ++pCurOutputPixel;
            }
        }
    }
}

//-------------------------------------------------------------------------------------------------
// Decode packed CEL pixel data.
// The pointer to the image data must point to the start data for the first row.
//-------------------------------------------------------------------------------------------------
static void decodePackedCelPixelData(
    const std::byte* const pImageData,
    const uint32_t imageDataSize,
    const uint16_t* const pPLUT,
    const uint16_t imageW,
    const uint16_t imageH,
    const uint8_t imageBPP,
    const bool bColorIndexed,
    uint16_t* const pImageOut
) THROWS {
    // Only supporting these image formats!
    const bool bSupportedImgFormat = (bColorIndexed || imageBPP == 16);

    if (!bSupportedImgFormat) {
        throw CelDecodeException();
    }

    // Alloc output image and start decoding each row
    const std::byte* pCurRowData = pImageData;
    const std::byte* pNextRowData = nullptr;

    for (uint16_t y = 0; y < imageH; ++y) {
        // Firstly read offset to the next row and store the pointer.
        // That is the first bit of info for each row of pixels.
        const uint32_t curOffsetInImgData = (uint32_t)(pCurRowData - pImageData);
        const uint32_t imageDataSizeLeft = (curOffsetInImgData < imageDataSize) ? imageDataSize - curOffsetInImgData : 0;
        BitStream bitStream(pCurRowData, imageDataSizeLeft);
        uint32_t nextRowOffset;

        {
            // For 8 and 16-bit CEL images the offset is encoded in 10-bits of a u16.
            // For other CEL image formats just a single byte is used.
            if (imageBPP >= 8) {
                nextRowOffset = bitStream.readBitsAsUInt<uint16_t>(16) & uint16_t(0x3FF);
            } else {
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

            if (packCount > imageW) {
                throw CelDecodeException();     // Bad image data!
            }

            if (packMode == CelPackMode::LITERAL) {
                // A number of literal pixels follow.
                // Read each pixel and output it to the output buffer:
                const uint16_t endX = x + packCount;

                while (x < endX) {
                    uint16_t color;

                    if (bColorIndexed) {
                        const uint16_t colorIdx = bitStream.readBitsAsUInt<uint16_t>(imageBPP);
                        color = Endian::bigToHost(pPLUT[colorIdx]) | OPAQUE_PIXEL_BITS;
                    } else {
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
                // A repeated pixel (should) follow
                if (packMode != CelPackMode::REPEAT) {
                    throw CelDecodeException();     // Bad image data!
                }

                uint16_t color;

                if (bColorIndexed) {
                    const uint16_t colorIdx = bitStream.readBitsAsUInt<uint16_t>(imageBPP);
                    color = Endian::bigToHost(pPLUT[colorIdx]) | OPAQUE_PIXEL_BITS;
                } else {
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

//----------------------------------------------------------------------------------------------------------------------
// Decodes the actual CEL pixel data.
// The pointer to the image data must point to the start data for the first row.
// If decoding fails then a null pointer will be returned.
//----------------------------------------------------------------------------------------------------------------------
static uint16_t* decodeCelPixelData(
    const std::byte* const pImageData,
    const uint32_t imageDataSize,
    const uint16_t* const pPLUT,
    const uint16_t imageW,
    const uint16_t imageH,
    const uint8_t imageBPP,
    const bool bImageIsPacked,
    const bool bColorIndexed
) noexcept {
    // Only supporting these image formats!
    const bool bSupportedImgFormat = (bColorIndexed || imageBPP == 16);

    if (!bSupportedImgFormat) {
        return nullptr;
    }

    // Alloc the pixels for the image
    uint16_t* const pImageOut = new uint16_t[imageW * imageH];

    // Try and do the decode
    try {
        if (bImageIsPacked) {
            decodePackedCelPixelData(
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
            decodeUnpackedCelPixelData(
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
    } catch (...) {
        delete[] pImageOut;
        return nullptr;
    }

    return pImageOut;
}

bool decodeCelImage(
    const CelControlBlock& ccb,
    const std::byte* const pImageData,
    const uint32_t imageDataSize,
    const uint16_t* const pPLUT,
    CelImage& imageOut
) noexcept {
    // Just in case something is already loaded into this image...
    imageOut.free();

    // Get image width and height
    const uint16_t imageW = getCCBWidth(ccb);
    const uint16_t imageH = getCCBHeight(ccb);

    // Image dimensions must be valid or decode fails
    if (imageW <= 0 || imageH <= 0)
        return false;
    
    // Get flags for the CCB
    const uint32_t imageCCBFlags = Endian::bigToHost(ccb.flags);
    
    // Get image bits per pixel. Note: don't support images with bit depth > 16 bpp!
    const uint8_t imageBPP = getCCBBitsPerPixel(ccb);

    if (imageBPP > 16)
        return false;
    
    // Determining whether the CCB is color indexed or not SHOULD be a simple case of checking the 'linear' flag but
    // sometimes this lies to us for some of the Doom resources.
    // Fix up some contradictions here...
    bool bIsColorIndexed = ((imageCCBFlags & CCB_FLAG_LINEAR) == 0);

    if (imageBPP < 8) {
        bIsColorIndexed = true;     // MUST be color indexed
    } else if (imageBPP >= 16) {
        bIsColorIndexed = false;    // MUST NOT be color indexed
    }

    // If the image is color indexed and a pixel LUT has not been provided then loading fails
    if (bIsColorIndexed && (!pPLUT))
        return false;
    
    // Decode and if the result is successful return also the image dimensions
    imageOut.pPixels = decodeCelPixelData(
        pImageData,
        imageDataSize,
        pPLUT,
        imageW,
        imageH,
        imageBPP,
        ((imageCCBFlags & CCB_FLAG_PACKED) != 0),
        bIsColorIndexed
    );

    if (imageOut.pPixels) {
        imageOut.width = imageW;
        imageOut.height = imageH;
        return true;
    } else {
        return false;
    }
}

uint16_t getCCBWidth(const CelControlBlock& ccb) noexcept {
    // DC: this logic comes from the original 3DO Doom source.
    // It can be found in the burgerlib function 'GetShapeWidth()':
    uint32_t width = Endian::bigToHost(ccb.pre1) & 0x7FF;               // Get the HCount bits
    width += 1;                                                         // Needed to get the TRUE result
    return width;
}

uint16_t getCCBHeight(const CelControlBlock& ccb) noexcept {
    // DC: this logic comes from the original 3DO Doom source.
    // It be found in the burgerlib function 'GetShapeHeight()':
    uint32_t height = Endian::bigToHost(ccb.pre0) >> 6;                 // Get the VCount bits
    height &= 0x3FF;                                                    // Mask off unused bits
    height += 1;                                                        // Needed to get the TRUE result
    return height;
}

bool loadRezFileCelImage(
    const std::byte* const pData,
    const uint32_t dataSize,
    const CelLoadFlags loadFlags,
    CelImage& imageOut
) noexcept {
    ASSERT(pData || dataSize == 0);

    // If the image has an x and a y offset then read that here first, otherwise they are just '0,0'
    const std::byte* pCurData = pData;

    if ((loadFlags & CelLoadFlagBits::HAS_OFFSETS) != 0) {
        if (dataSize <= 4) {
            return false;   // Not big enough!
        }

        const int16_t* const pOffsets = (const int16_t*) pCurData;
        imageOut.offsetX = Endian::bigToHost(pOffsets[0]);
        imageOut.offsetY = Endian::bigToHost(pOffsets[1]);
        pCurData += sizeof(int16_t) * 2;
    } else {
        imageOut.offsetX = 0;
        imageOut.offsetY = 0;
    }

    // Read the actual image data
    const uint32_t bytesConsumedSoFar = (uint32_t)(pCurData - pData);
    const uint32_t celDataSize = dataSize - bytesConsumedSoFar;

    // If the data size is not big enough then decode fails
    if (celDataSize <= sizeof(CelControlBlock))
        return false;

    if (celDataSize <= 60)  // Ensure there is room for the PLUT
        return false;
    
    // Get the offset to the image data for the CCB; ensure this offset is valid:
    const std::byte* const pCelBytes = (const std::byte*) pCurData;
    const CelControlBlock& ccb = reinterpret_cast<const CelControlBlock&>(*pCelBytes);
    const uint32_t imageDataOffset = Endian::bigToHost(ccb.sourcePtr) + 12;

    if (imageDataOffset >= celDataSize)
        return false;
    
    // Get the pointers to the PLUT and image data
    const uint16_t* const pPLUT = (const uint16_t*)(pCelBytes + 60);    // 3DO Doom harcoded this offset?
    const std::byte* const pImageData = pCelBytes + imageDataOffset;
    const uint32_t imageDataSize = celDataSize - (uint32_t)(pImageData - pCelBytes);

    // Decode the actual CEL image data
    const bool bSuccess = decodeCelImage(
        ccb,
        pImageData,
        imageDataSize,
        pPLUT,
        imageOut
    );

    // Do conversions for 'masked' images if we have loaded successfully and that is required
    if (bSuccess) {
        if ((loadFlags & CelLoadFlagBits::MASKED) != 0) {
            transformMaskedImageToImageWithAlpha(imageOut);
        }
    } else {
        imageOut.free();
    }

    return bSuccess;
}

bool loadRezFileCelImages(
    const std::byte* const pData,
    const uint32_t dataSize,
    const CelLoadFlags loadFlags,
    CelImageArray& imagesOut
) noexcept {
    ASSERT(pData || dataSize == 0);

    // Make sure there is at least 1 u32 in the stream!
    if (dataSize <= 4)
        return false;
    
    // The offset to each image in the array follows as an array.
    // We can tell the size of the image offsets array by the offset to the first image:
    const uint32_t numImages = Endian::bigToHost(((const uint32_t*) pData)[0]) / sizeof(uint32_t);

    // Make sure we can read each offset and that there is data after:
    if (dataSize <= numImages * 4)
        return false;
    
    // Read each image offset and consequently each image
    bool bAllSucceeded = true;

    imagesOut.numImages = numImages;
    imagesOut.pImages = new CelImage[numImages];

    for (uint32_t imageIdx = 0; imageIdx < numImages; ++imageIdx) {
        // Figure out where this image in the array starts and ends
        const uint32_t thisImageOffset = Endian::bigToHost(((const uint32_t*) pData)[imageIdx]);
        const uint32_t nextImageOffset = (imageIdx + 1 < numImages) ? Endian::bigToHost(((const uint32_t*) pData)[imageIdx + 1]) : dataSize;
        const uint32_t thisImageDataSize = nextImageOffset - thisImageOffset;

        // Make sure there is enough data left in the stream before we read the image!
        if (thisImageOffset >= dataSize || thisImageOffset + thisImageDataSize > dataSize) {
            bAllSucceeded = false;
            break;
        }

        // Load the image and abort if it failed
        const bool bImageLoadSucceeded = loadRezFileCelImage(
            pData + thisImageOffset,
            thisImageDataSize,
            loadFlags,
            imagesOut.pImages[imageIdx]
        );

        if (!bImageLoadSucceeded) {
            bAllSucceeded = false;
            break;
        }
    }

    // If the load failed then cleanup, otherwise save the load flags for future reference
    if (bAllSucceeded) {
        imagesOut.loadFlags = loadFlags;
        return true;
    } else {
        imagesOut.free();
        return false;
    }
}

END_NAMESPACE(CelUtils)
