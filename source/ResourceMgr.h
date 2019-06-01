#pragma once

#include <cstdio>
#include <cstdint>
#include <vector>

struct Resource;

//---------------------------------------------------------------------------------------------------------------------
// Manages a 3DO doom resource file and the resources within it
//---------------------------------------------------------------------------------------------------------------------
class ResourceMgr {
public:
    ResourceMgr() noexcept;
    ~ResourceMgr() noexcept;
    
    void init(const char* const fileName) noexcept;
    void destroy() noexcept;
    
    const Resource* getResource(const uint32_t number) const noexcept;
    const Resource* loadResource(const uint32_t number) noexcept;
    const Resource* freeResource(const uint32_t number) noexcept;
    
private:
    struct ResourceFileHeader {
        std::byte   magic[4];               // Should read 'BRGR'
        uint32_t    numResourceGroups;
        uint32_t    resourceGroupHeadersSize;
    };
    
    struct ResourceGroupHeader {
        uint32_t    resourceType;
        uint32_t    resourcesStartNum;
        uint32_t    numResources;
    };
    
    struct ResourceHeader {
        uint32_t    offset;
        uint32_t    size;
        uint32_t    unused;
    };
    
    static bool compareResourcesByNumber(const Resource& r1, const Resource& r2) noexcept;
    
    static void freeResource(Resource& resource) noexcept;
    void freeAllResources() noexcept;
    
    inline Resource* getMutableResource(const uint32_t number) noexcept {
        // Note: const casting to save duplicating the same code!
        return const_cast<Resource*>(getResource(number));
    }
    
    FILE*                   mpResourceFile;
    std::vector<Resource>   mResources;
};
