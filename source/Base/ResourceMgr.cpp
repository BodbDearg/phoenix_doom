#include "ResourceMgr.h"

#include "Endian.h"
#include "FourCID.h"
#include "Mem.h"
#include "Resource.h"
#include <algorithm>

struct ResourceFileHeader {
    FourCID     magic;  // Should read 'BRGR'
    uint32_t    numResourceGroups;
    uint32_t    resourceGroupHeadersSize;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(numResourceGroups);
        Endian::convertBigToHost(resourceGroupHeadersSize);
    }
};

struct ResourceGroupHeader {
    uint32_t    resourceType;
    uint32_t    resourcesStartNum;
    uint32_t    numResources;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(resourceType);
        Endian::convertBigToHost(resourcesStartNum);
        Endian::convertBigToHost(numResources);
    }
};

struct ResourceHeader {
    uint32_t    offset;
    uint32_t    size;
    uint32_t    _unused;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(offset);
        Endian::convertBigToHost(size);
    }
};

ResourceMgr::ResourceMgr() noexcept
    : mpResourceFile(nullptr)
    , mResources()
    , mEndResourceNum(0)
{
}

ResourceMgr::~ResourceMgr() noexcept {
    destroy();
}

void ResourceMgr::init(const char* const fileName) noexcept {
    // Preconditions: file must be specified, must not already be initialized
    ASSERT(fileName);
    ASSERT(mpResourceFile == nullptr);

    // Open the resource file
    mpResourceFile = GameDataFS::openFile(fileName);

    if (!mpResourceFile) {
        FATAL_ERROR_F("ERROR: Unable to open game resource file '%s'!", fileName);
    }

    // Read the file header and verify
    ResourceFileHeader fileHeader = {};

    try {
        mpResourceFile->read(fileHeader);
    } catch (...) {
        FATAL_ERROR_F("ERROR: Failed to read game resource file '%s' header!", fileName);
    }

    fileHeader.convertBigToHostEndian();

    const bool bHeaderOk = (
        (fileHeader.magic == FourCID("BRGR")) &&
        (fileHeader.numResourceGroups > 0) &&
        (fileHeader.resourceGroupHeadersSize > 0)
    );

    if (!bHeaderOk) {
        FATAL_ERROR_F("ERROR: Game resource file '%s' has an invalid header!", fileName);
    }

    // Now read all of the resource group and individual resource headers
    std::unique_ptr<std::byte[]> pResourceHeadersData(new std::byte[fileHeader.resourceGroupHeadersSize]);

    try {
        mpResourceFile->readBytes(pResourceHeadersData.get(), fileHeader.resourceGroupHeadersSize);
    } catch (...) {
        FATAL_ERROR_F("ERROR: Failed to read game resource file '%s' header!", fileName);
    }

    mResources.reserve((size_t) fileHeader.numResourceGroups * 4);

    {
        const std::byte* pCurBytes = pResourceHeadersData.get();
        const std::byte* const pEndBytes = pCurBytes + fileHeader.resourceGroupHeadersSize;

        while (pCurBytes + sizeof(ResourceGroupHeader) <= pEndBytes) {
            ResourceGroupHeader* const pGroupHeader = (ResourceGroupHeader*) pCurBytes;
            pGroupHeader->convertBigToHostEndian();
            pCurBytes += sizeof(ResourceGroupHeader);

            // Figure out the end resource number for this group and contribute to the max for the manager
            uint32_t resourceNum = pGroupHeader->resourcesStartNum;
            const uint32_t endResourceNum = resourceNum + pGroupHeader->numResources;
            mEndResourceNum = std::max(endResourceNum, mEndResourceNum);

            // Create all the resources in the group
            while (resourceNum < endResourceNum) {
                if (pCurBytes + sizeof(ResourceHeader) > pEndBytes) {
                    FATAL_ERROR_F("Resource file '%s' is invalid! Unexpected head of resource header data!", fileName);
                }

                ResourceHeader* const pResourceHeader = (ResourceHeader*) pCurBytes;
                pResourceHeader->convertBigToHostEndian();
                pCurBytes += sizeof(ResourceHeader);

                // Burgerlib used '0x80000000' to encode a 'fixed handle' (never unloaded) flag in the offset.
                // It seemed to also reserve other bits by masking by 0x3FFFFFFF - do the same here...
                pResourceHeader->offset &= 0x3FFFFFFF;

                Resource& resource = mResources.emplace_back();
                resource.number = resourceNum;
                resource.type = pGroupHeader->resourceType;
                resource.offset = pResourceHeader->offset;
                resource.size = pResourceHeader->size;

                ++resourceNum;
            }
        }
    }

    // Sort all of the resource headers so they can be binary searched
    std::sort(mResources.begin(), mResources.end(), compareResourcesByNumber);
}

void ResourceMgr::destroy() noexcept {
    mEndResourceNum = 0;
    freeAllResources();
    mResources.clear();
    mpResourceFile.reset();
}

const Resource* ResourceMgr::getResource(const uint32_t number) const noexcept {
    Resource searchValue;
    searchValue.number = number;
    auto iter = std::lower_bound(mResources.begin(), mResources.end(), searchValue, compareResourcesByNumber);

    if (iter != mResources.end()) {
        const Resource* const pResourceHeader = &(*iter);
        return (pResourceHeader->number == number) ? pResourceHeader : nullptr;
    }

    return nullptr;
}

const Resource* ResourceMgr::loadResource(const uint32_t number) noexcept {
    ASSERT(mpResourceFile);
    Resource* const pResource = getMutableResource(number);

    if (!pResource) {
        FATAL_ERROR_F("Invalid resource number to load: %u!", unsigned(number));
    }

    if (!pResource->pData) {
        pResource->pData = MemAlloc(pResource->size);

        try {
            mpResourceFile->seek(pResource->offset);
            mpResourceFile->readBytes(pResource->pData, pResource->size);
        } catch (...) {
            FATAL_ERROR_F("Failed to read resource number %u!", unsigned(number));
        }
    }

    return pResource;
}

const Resource* ResourceMgr::freeResource(const uint32_t number) noexcept {
    Resource* const pResource = getMutableResource(number);

    if (pResource) {
        freeResource(*pResource);
    }

    return pResource;
}

bool ResourceMgr::compareResourcesByNumber(const Resource& r1, const Resource& r2) noexcept {
    return (r1.number < r2.number);
}

void ResourceMgr::freeResource(Resource& resource) noexcept {
    MEM_FREE_AND_NULL(resource.pData);
}

void ResourceMgr::freeAllResources() noexcept {
    for (Resource& resource : mResources) {
        freeResource(resource);
    }
}
