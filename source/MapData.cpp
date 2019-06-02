#include "MapData.h"

#include "doomrez.h"
#include "Endian.h"
#include "Resources.h"
#include <vector>

// lump order in a map wad
enum {
    ML_THINGS,
    ML_LINEDEFS,
    ML_SIDEDEFS,
    ML_VERTEXES,
    ML_SEGS,
    ML_SSECTORS,
    ML_SECTORS,
    ML_NODES,
    ML_REJECT,
    ML_BLOCKMAP,
    ML_TOTAL
};

// Internal vertex arrays
static std::vector<vertex_t> gVertexes;

//---------------------------------------------------------------------------------------------------------------------
// Load vertex data
//---------------------------------------------------------------------------------------------------------------------
static void loadVertexes(const uint32_t resourceNum) noexcept {
    const Resource* const pVertsResource = loadResource(resourceNum);
    
    const uint32_t numVerts = pVertsResource->size / sizeof(vertex_t);
    gVertexes.resize(numVerts);
    gpVertexes = gVertexes.data();
    
    const vertex_t* pSrcVert = (const vertex_t*) pVertsResource->pData;
    const vertex_t* const pEndSrcVerts = (const vertex_t*) pVertsResource->pData + numVerts;
    vertex_t* pDstVert = gVertexes.data();
    
    while (pSrcVert < pEndSrcVerts) {
        pDstVert->x = byteSwappedI32(pSrcVert->x);
        pDstVert->y = byteSwappedI32(pSrcVert->y);
        ++pSrcVert;
        ++pDstVert;
    }
    
    releaseResource(resourceNum);
}

#ifdef __cplusplus
extern "C" {
#endif

// External data pointers
const vertex_t* gpVertexes;

void mapDataInit(const uint32_t mapNum) {
    // Figure out the resource number for the first map lump
    const uint32_t mapStartLump = ((mapNum - 1) * ML_TOTAL) + rMAP01;

    // Load all the map data
    loadVertexes(mapStartLump + ML_VERTEXES);
}

void mapDataShutdown() {
    gVertexes.clear();
    gpVertexes = nullptr;
}

#ifdef __cplusplus
}
#endif
