#include "CelImages.h"

#include "Base/Endian.h"
#include "Base/Mem.h"
#include "Base/Resource.h"
#include "CelUtils.h"
#include "Game/Resources.h"
#include <vector>

BEGIN_NAMESPACE(CelImages)

// Note: I'm using a flat vector here instead of something like a map because it offers constant time access.
// There is one array slot here per resource in the resource manager. This might seem wasteful of memory but
// there are not that many resources in the resource file and the 'CelImageArray' struct is small, so the
// tradeoff is probably worth it.
static std::vector<CelImageArray> gImageArrays;

static void freeImage(CelImage& image) noexcept {
    image.width = 0;
    image.height = 0;
    image.offsetX = 0;
    image.offsetY = 0;
    delete[] image.pPixels;
}

static void freeImageArray(CelImageArray& imageArray) noexcept {
    if (imageArray.pImages) {
        uint32_t numImages = imageArray.numImages;

        while (numImages > 0) {
            --numImages;
            freeImage(imageArray.pImages[numImages]);
        }

        delete[] imageArray.pImages;
    }

    imageArray.numImages = 0;
    imageArray.loadFlags = 0;
}

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

static void loadImage(
    CelImage& image,
    const std::byte* const pData,
    const uint32_t dataSize,
    const LoadFlags loadFlags
) noexcept {
    // If the image has an x and a y offset then read that here first, otherwise they are just '0,0'
    const std::byte* pCurData = pData;

    if ((loadFlags & LoadFlagBits::HAS_OFFSETS) != 0) {
        const int16_t* const pOffsets = (const int16_t*) pCurData;
        image.offsetX = byteSwappedI16(pOffsets[0]);
        image.offsetY = byteSwappedI16(pOffsets[1]);
        pCurData += sizeof(int16_t) * 2;
    } else {
        image.offsetX = 0;
        image.offsetY = 0;
    }

    // Read the actual image data and convert a masked image to an image with alpha if we have to
    CelUtils::decodeDoomCelSprite(
        (const CelControlBlock*) pCurData,
        dataSize,
        &image.pPixels,
        &image.width,
        &image.height
    );

    if ((loadFlags & LoadFlagBits::MASKED) != 0) {
        transformMaskedImageToImageWithAlpha(image);
    }
}

static void loadImages(
    CelImageArray& imageArray,
    const uint32_t resourceNum,
    const LoadFlags loadFlags,
    const bool bLoadImageArray
) noexcept {
    // Firstly load up the resource.
    // Note: don't need to check for an error since if this fails it will be a fatal error!
    ASSERT(!imageArray.pImages);
    const Resource* const pResource = Resources::load(resourceNum);
    const std::byte* const pResourceData = pResource->pData;
    const uint32_t resourceSize = pResource->size;

    // Read each individual image.
    // Note: could be dealing with an array of images or just one.
    imageArray.loadFlags = loadFlags;

    if (bLoadImageArray) {
        // In the case of an image array we can tell the number of images from the size of the image offsets array at the start.
        // We can tell the size of the image offsets array by the offset to the first image:
        const uint32_t numImages = byteSwappedU32(((const uint32_t*) pResourceData)[0]) / sizeof(uint32_t);
        imageArray.numImages = numImages;
        imageArray.pImages = new CelImage[numImages];

        for (uint32_t imageIdx = 0; imageIdx < numImages; ++imageIdx) {
            const uint32_t thisImageOffset = byteSwappedU32(((const uint32_t*) pResourceData)[imageIdx]);
            const uint32_t nextImageOffset = (imageIdx + 1 < numImages) ? byteSwappedU32(((const uint32_t*) pResourceData)[imageIdx + 1]) : resourceSize;
            const uint32_t thisImageDataSize = nextImageOffset - thisImageOffset;

            loadImage(imageArray.pImages[imageIdx], pResourceData + thisImageOffset, thisImageDataSize, loadFlags);
        }
    } else {
        // Loading a single image
        imageArray.numImages = 1;
        imageArray.pImages = new CelImage[1];
        loadImage(imageArray.pImages[0], pResourceData, resourceSize, loadFlags);
    }

    // After we are done we can free the raw resource - done at this point
    Resources::free(resourceNum);
}

void init() noexcept {
    // Expect the resource manager to be initialized and for this module to NOT be initialized
    ASSERT(gImageArrays.empty());
    ASSERT(Resources::getEndResourceNum() > 0);

    // Alloc room for each potential image (one per resource, even though not all resources are CEL images)
    gImageArrays.resize(Resources::getEndResourceNum());
}

void shutdown() noexcept {
    freeAll();
    gImageArrays.clear();
}

void freeAll() noexcept {
    for (CelImageArray& imageArray : gImageArrays) {
        freeImageArray(imageArray);
    }
}

const CelImageArray* getImages(const uint32_t resourceNum) noexcept {
    if (resourceNum < (uint32_t) gImageArrays.size()) {
        return &gImageArrays[resourceNum];
    } else {
        return nullptr;
    }
}

const CelImage* getImage(const uint32_t resourceNum) noexcept {
    const CelImageArray* const pImageArray = getImages(resourceNum);

    if (pImageArray && resourceNum < pImageArray->numImages) {
        return &pImageArray->pImages[0];
    }

    return nullptr;
}

const CelImageArray& loadImages(const uint32_t resourceNum, const LoadFlags loadFlags) noexcept {
    if (resourceNum >= (uint32_t) gImageArrays.size()) {
        FATAL_ERROR_F("Invalid resource number '%u': unable to load this resource!", resourceNum);
    }

    CelImageArray& imageArray = gImageArrays[resourceNum];

    if (!imageArray.pImages) {
        loadImages(imageArray, resourceNum, loadFlags, true);
    }

    return imageArray;
}

const CelImage& loadImage(const uint32_t resourceNum, const LoadFlags loadFlags) noexcept {
    if (resourceNum >= (uint32_t) gImageArrays.size()) {
        FATAL_ERROR_F("Invalid resource number '%u': unable to load this resource!", resourceNum);
    }

    CelImageArray& imageArray = gImageArrays[resourceNum];

    if (!imageArray.pImages) {
        loadImages(imageArray, resourceNum, loadFlags, false);
    }

    return imageArray.pImages[0];
}

void freeImages(const uint32_t resourceNum) noexcept {
    if (resourceNum < (uint32_t) gImageArrays.size()) {
        freeImageArray(gImageArrays[resourceNum]);
    }
}

void releaseImages(const uint32_t resourceNum) noexcept {
    // Note: function does nothing at the moment deliberately, just here as a statement of intent.
    // For why this is, see 'Resources::release()'.
    MARK_UNUSED(resourceNum);
}

END_NAMESPACE(CelImages)
