#include "CelImages.h"

#include "Base/Resource.h"
#include "Game/Resources.h"
#include <vector>

BEGIN_NAMESPACE(CelImages)

// Note: I'm using a flat vector here instead of something like a map because it offers constant time access.
// There is one array slot here per resource in the resource manager. This might seem wasteful of memory but
// there are not that many resources in the resource file and the 'CelImageArray' struct is small, so the
// tradeoff is probably worth it.
static std::vector<CelImageArray> gImageArrays;

static void loadImages(
    CelImageArray& imageArray,
    const uint32_t resourceNum,
    const CelLoadFlags loadFlags,
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
    if (bLoadImageArray) {
        if (!CelUtils::loadCelImages(pResourceData, resourceSize, loadFlags, imageArray)) {
            FATAL_ERROR("Failed to load a CEL format image used by the game!");
        }
    } else {
        imageArray.numImages = 1;
        imageArray.loadFlags = loadFlags;
        imageArray.pImages = new CelImage[1];

        if (!CelUtils::loadCelImage(pResourceData, resourceSize, loadFlags, imageArray.pImages[0])) {
            FATAL_ERROR("Failed to load a CEL format image used by the game!");
        }
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
        imageArray.free();
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

const CelImageArray& loadImages(const uint32_t resourceNum, const CelLoadFlags loadFlags) noexcept {
    if (resourceNum >= (uint32_t) gImageArrays.size()) {
        FATAL_ERROR_F("Invalid resource number '%u': unable to load this resource!", resourceNum);
    }

    CelImageArray& imageArray = gImageArrays[resourceNum];

    if (!imageArray.pImages) {
        loadImages(imageArray, resourceNum, loadFlags, true);
    }

    return imageArray;
}

const CelImage& loadImage(const uint32_t resourceNum, const CelLoadFlags loadFlags) noexcept {
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
        gImageArrays[resourceNum].free();
    }
}

void releaseImages(const uint32_t resourceNum) noexcept {
    // Note: function does nothing at the moment deliberately, just here as a statement of intent.
    // For why this is, see 'Resources::release()'.
    MARK_UNUSED(resourceNum);
}

END_NAMESPACE(CelImages)
