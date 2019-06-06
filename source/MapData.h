#pragma once

#include "doom.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pointers to global map data for ease of access
extern const vertex_t*      gpVertexes;
extern uint32_t             gNumVertexes;
extern sector_t*            gpSectors;
extern uint32_t             gNumSectors;
extern side_t*              gpSides;
extern uint32_t             gNumSides;
extern line_t*              gpLines;
extern uint32_t             gNumLines;
extern const seg_t*         gpLineSegs;
extern uint32_t             gNumLineSegs;
extern const subsector_t*   gpSubSectors;
extern uint32_t             gNumSubSectors;
extern const node_t*        gpBSPTreeRoot;
extern const Byte*          gpRejectMatrix;         // For fast sight rejection
extern line_t***            gpBlockMapLineLists;    // For each blockmap entry, a pointer to a list of line pointers (all lines in the block)
extern mobj_t**             gpBlockMapThingLists;   // For each blockmap entry, a pointer to the first thing in a linked list of things (all things in the block)
extern uint32_t             gBlockMapWidth;
extern uint32_t             gBlockMapHeight;
extern Fixed                gBlockMapOriginX;
extern Fixed                gBlockMapOriginY;

// Load all map data for the specified map and release it
void mapDataInit(const uint32_t mapNum);
void mapDataShutdown();

#ifdef __cplusplus
}
#endif
