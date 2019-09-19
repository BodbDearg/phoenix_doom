#include "Resources.h"

#include "Base/Resource.h"
#include "Base/ResourceMgr.h"

BEGIN_NAMESPACE(Resources)

static constexpr const char* const RESOURCE_FILE_PATH = "REZFILE";

static ResourceMgr gResourceMgr;

void init() noexcept {
    gResourceMgr.init(RESOURCE_FILE_PATH);
}

void shutdown() noexcept {
    gResourceMgr.destroy();
}

const Resource* get(const uint32_t num) noexcept {
    return gResourceMgr.getResource(num);
}

std::byte* getData(const uint32_t num) noexcept {
    const Resource* pResource = get(num);
    return (pResource != nullptr) ? pResource->pData : nullptr;
}

const Resource* load(const uint32_t num) noexcept {
    return gResourceMgr.loadResource(num);
}

std::byte* loadData(const uint32_t num) noexcept {
    const Resource* pResource = load(num);
    return (pResource != nullptr) ? pResource->pData : nullptr;
}

void free(const uint32_t num) noexcept {
    gResourceMgr.freeResource(num);
}

void release([[maybe_unused]] const uint32_t num) noexcept {
    // At the moment I'm not implementing any kind of mark and purge memory management system like what
    // Burgerlib had in the original 3DO source - this call is merely for documentation purposes throughout
    // the code as a statement of intent. The code can still use this call to say 'Hey, I don't need this
    // data anymore - get rid of it if you like'.
    //
    // Since modern PCs have so much memory and the data size of the 3DO Doom resources file is only ~4MB
    // a perfectly valid memory management strategy is to simply NOT manage memory and hold onto everything.
    // Should make level transitions faster - not that they would be slow anyway...
    //
    // If you're using this code to do something a little more demanding however then you may want to revisit this.
}

uint32_t getEndResourceNum() noexcept {
    return gResourceMgr.getEndResourceNum();
}

END_NAMESPACE(Resources)
