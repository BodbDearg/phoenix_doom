#include "Resources.h"

#include "Base/ResourceMgr.h"

static const char* const RESOURCE_FILE_PATH = "REZFILE";
static ResourceMgr gResourceMgr;

void resourcesInit() {
    gResourceMgr.init(RESOURCE_FILE_PATH);
}

void resourcesShutdown() {
    gResourceMgr.destroy();
}

const Resource* getResource(const uint32_t num) {
    return gResourceMgr.getResource(num);
}

std::byte* getResourceData(const uint32_t num) {
    const Resource* pResource = getResource(num);
    return (pResource != nullptr) ? pResource->pData : nullptr;
}

const Resource* loadResource(const uint32_t num) {
    return gResourceMgr.loadResource(num);
}

std::byte* loadResourceData(const uint32_t num) {
    const Resource* pResource = loadResource(num);
    return (pResource != nullptr) ? pResource->pData : nullptr;
}

void freeResource(const uint32_t num) {
    gResourceMgr.freeResource(num);
}

void releaseResource(const uint32_t num) {
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
