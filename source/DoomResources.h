#pragma once

#include "Resource.h"

#ifdef __cplusplus
extern "C" {
#endif

void initDoomResources();
void shutdownDoomResources();
    
const struct Resource* getDoomResource(const uint32_t num);
void* getDoomResourceData(const uint32_t num);

const struct Resource* loadDoomResource(const uint32_t num);
void* loadDoomResourceData(const uint32_t num);

const struct Resource* freeDoomResource(const uint32_t num);
const struct Resource* releaseDoomResource(const uint32_t num);
        
#ifdef __cplusplus
}
#endif
