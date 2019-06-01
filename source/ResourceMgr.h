#pragma once

#include <cstdint>
#include <cstdio>
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
