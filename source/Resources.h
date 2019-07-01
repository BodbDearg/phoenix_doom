#pragma once

#include "Resource.h"

void resourcesInit();
void resourcesShutdown();
    
const Resource* getResource(const uint32_t num);
std::byte* getResourceData(const uint32_t num);

const Resource* loadResource(const uint32_t num);
std::byte* loadResourceData(const uint32_t num);

void freeResource(const uint32_t num);
void releaseResource(const uint32_t num);
