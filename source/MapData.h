#pragma once

#include "doom.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pointers to global map data for ease of access */
extern const vertex_t* gpVertexes;

/* Load all map data for the specified map and release it */
void mapDataInit(const uint32_t mapNum);
void mapDataShutdown();

#ifdef __cplusplus
}
#endif
