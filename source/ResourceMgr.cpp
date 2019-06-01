#include "ResourceMgr.h"

#include "Macros.h"
#include "Mem.h"
#include "Resource.h"
#include <memory>

static void swapEndian(uint32_t& num) noexcept {
    num = (
        ((num & 0x000000FFU) << 24) |
        ((num & 0x0000FF00U) << 8) |
        ((num & 0x00FF0000U) >> 8) |
        ((num & 0xFF000000U) >> 24)
    );
}

ResourceMgr::ResourceMgr() noexcept
    : mpResourceFile(nullptr)
    , mResources()
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
    mpResourceFile = std::fopen(fileName, "rb");
    
    if (!mpResourceFile) {
        FATAL_ERROR("ERROR: Unable to open game resource file '%s'!\n", fileName);
    }
    
    // Read the file header and verify
    ResourceFileHeader fileHeader = {};
    
    if (std::fread(&fileHeader, sizeof(ResourceFileHeader), 1, mpResourceFile) != 1) {
        FATAL_ERROR("ERROR: Failed to read game resource file '%s' header!\n", fileName);
    }
    
    swapEndian(fileHeader.numResourceGroups);
    swapEndian(fileHeader.resourceGroupHeadersSize);
    
    const bool bHeaderOk = (
        (fileHeader.magic[0] == std::byte('B')) &&
        (fileHeader.magic[1] == std::byte('R')) &&
        (fileHeader.magic[2] == std::byte('G')) &&
        (fileHeader.magic[3] == std::byte('R')) &&
        (fileHeader.numResourceGroups > 0) &&
        (fileHeader.resourceGroupHeadersSize > 0)
    );
    
    if (!bHeaderOk) {
        FATAL_ERROR("ERROR: Game resource file '%s' has an invalid header!\n", fileName);
    }
    
    // Now read all of the resource group and individual resource headers
    std::unique_ptr<std::byte[]> pResourceHeadersData(new std::byte[fileHeader.resourceGroupHeadersSize]);
    
    if (std::fread(pResourceHeadersData.get(), fileHeader.resourceGroupHeadersSize, 1, mpResourceFile) != 1) {
        FATAL_ERROR("ERROR: Failed to read game resource file '%s' header!\n", fileName);
    }
    
    mResources.reserve(fileHeader.numResourceGroups * 4);
    
    {
        const std::byte* pCurBytes = pResourceHeadersData.get();
        const std::byte* const pEndBytes = pCurBytes + fileHeader.resourceGroupHeadersSize;
        
        while (pCurBytes + sizeof(ResourceGroupHeader) <= pEndBytes) {
            ResourceGroupHeader* const pGroupHeader = (ResourceGroupHeader*) pCurBytes;
            swapEndian(pGroupHeader->resourceType);
            swapEndian(pGroupHeader->resourcesStartNum);
            swapEndian(pGroupHeader->numResources);
            
            pCurBytes += sizeof(ResourceGroupHeader);
            
            uint32_t resourceNum = pGroupHeader->resourcesStartNum;
            const uint32_t endResourceNum = resourceNum + pGroupHeader->numResources;
            
            while (resourceNum < endResourceNum && pCurBytes + sizeof(ResourceHeader) <= pEndBytes) {
                ResourceHeader* const pResourceHeader = (ResourceHeader*) pCurBytes;
                swapEndian(pResourceHeader->offset);
                swapEndian(pResourceHeader->size);
                
                // Burgerlib used '0x80000000' to encode a 'fixed handle' (never unloaded) flag in the offset.
                // It seemed to reserve other bits by masking by 0x3FFFFFFF - do the same here...
                pResourceHeader->offset &= 0x3FFFFFFF;
                
                pCurBytes += sizeof(ResourceHeader);
                
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
    if (mpResourceFile) {
        std::fclose(mpResourceFile);
        mpResourceFile = nullptr;
    }
    
    freeAllResources();
    mResources.clear();
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
    
    if (pResource) {
        if (!pResource->pData) {
            pResource->pData = MemAlloc(pResource->size);
            
            if (std::fread(pResource->pData, pResource->size, 1, mpResourceFile) != 1) {
                FATAL_ERROR("Failed to read resource number %u!", unsigned(number));
            }
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
