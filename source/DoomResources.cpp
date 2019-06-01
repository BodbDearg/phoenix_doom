#include "DoomResources.h"

#include "ResourceMgr.h"

extern "C" {

static const char* const RESOURCE_FILE_PATH = "REZFILE";

static ResourceMgr gResourceMgr;

void initDoomResources() {
    gResourceMgr.init(RESOURCE_FILE_PATH);
}

void shutdownDoomResources() {
    gResourceMgr.destroy();
}

const Resource* getDoomResource(const uint32_t num) {
    return gResourceMgr.getResource(num);
}

void* getDoomResourceData(const uint32_t num) {
    const Resource* pResource = getDoomResource(num);
    return (pResource != nullptr) ? pResource->pData : nullptr;
}

const Resource* loadDoomResource(const uint32_t num) {
    return gResourceMgr.loadResource(num);
}

void* loadDoomResourceData(const uint32_t num) {
    const Resource* pResource = loadDoomResource(num);
    return (pResource != nullptr) ? pResource->pData : nullptr;
}

const Resource* freeDoomResource(const uint32_t num) {
    return gResourceMgr.freeResource(num);
}

const Resource* releaseDoomResource(const uint32_t num) {
    // TODO: Implement a mark & purge system like what Burgerlib had, or does it even matter?
    // At the moment I'm just ignoring this call.
    return gResourceMgr.getResource(num);
}

}
