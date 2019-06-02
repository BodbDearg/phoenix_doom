#pragma once

#include "Resource.h"

#ifdef __cplusplus
extern "C" {
#endif

void resourcesInit();
void resourcesShutdown();
    
const struct Resource* getResource(const uint32_t num);
void* getResourceData(const uint32_t num);

const struct Resource* loadResource(const uint32_t num);
void* loadResourceData(const uint32_t num);

void freeResource(const uint32_t num);
void releaseResource(const uint32_t num);
        
#ifdef __cplusplus
}
#endif
