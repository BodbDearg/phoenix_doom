#include "MapData.h"

#include "doomrez.h"
#include "Endian.h"
#include "Resources.h"
#include <vector>

// Struct for a map sector loaded from disk
struct MapSector {
    Fixed       floorHeight;
    Fixed       ceilingHeight;
    uint32_t    floorPic;           // Floor flat number
    uint32_t    ceilingPic;         // Ceiling flat number
    uint32_t    lightLevel;
    uint32_t    special;            // Special flags
    uint32_t    tag;                // Tag ID
};

// Order of the lumps in a Doom wad (and also the 3DO resource file)
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
static std::vector<sector_t> gSectors;

//---------------------------------------------------------------------------------------------------------------------
// Load vertex data
//---------------------------------------------------------------------------------------------------------------------
static void loadVertexes(const uint32_t lumpResourceNum) noexcept {
    const Resource* const pVertsResource = loadResource(lumpResourceNum);
    
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
    
    releaseResource(lumpResourceNum);
}

//---------------------------------------------------------------------------------------------------------------------
// Load sector data
//---------------------------------------------------------------------------------------------------------------------
static void loadSectors(const uint32_t lumpResourceNum) noexcept {
    // Load the sectors resource
    const Resource* const pSectorsResource = loadResource(lumpResourceNum);
    const std::byte* const pResourceData = (const std::byte*) pSectorsResource->pData;
    
    // Get the number of sectors first (first u32)
    const uint32_t numSectors = byteSwappedU32(((const uint32_t*) pResourceData)[0]);
    gNumSectors = numSectors;
    
    // Get the source sector data and alloc room for the runtime sectors
    const MapSector* pSrcSector = (const MapSector*)(pResourceData + sizeof(uint32_t));
    const MapSector* const pEndSrcSectors = pSrcSector + numSectors;
    
    gSectors.clear();
    gSectors.resize(numSectors);
    gpSectors = gSectors.data();
    
    // Create all sectors
    sector_t* pDstSector = gSectors.data();
    
    while (pSrcSector < pEndSrcSectors) {
        pDstSector->floorheight = byteSwappedI32(pSrcSector->floorHeight);
        pDstSector->ceilingheight = byteSwappedI32(pSrcSector->ceilingHeight);
        pDstSector->FloorPic = byteSwappedU32(pSrcSector->floorPic);
        pDstSector->CeilingPic = byteSwappedU32(pSrcSector->ceilingPic);
        pDstSector->lightlevel = byteSwappedU32(pSrcSector->lightLevel);
        pDstSector->special = byteSwappedU32(pSrcSector->special);
        pDstSector->tag  = byteSwappedU32(pSrcSector->tag);
        
        ++pSrcSector;
        ++pDstSector;
    }
    
    // Don't need this anymore
    releaseResource(lumpResourceNum);
}

#ifdef __cplusplus
extern "C" {
#endif

// External data pointers and information
const vertex_t*     gpVertexes;
uint32_t            gNumSectors;
sector_t*           gpSectors;

void mapDataInit(const uint32_t mapNum) {
    // Figure out the resource number for the first map lump
    const uint32_t mapStartLump = ((mapNum - 1) * ML_TOTAL) + rMAP01;
    
    // Load all the map data.
    // N.B: must be done in this order due to data dependencies!
    loadVertexes(mapStartLump + ML_VERTEXES);
    loadSectors(mapStartLump + ML_SECTORS);
}

void mapDataShutdown() {
    gVertexes.clear();
    gpVertexes = nullptr;
    
    gSectors.clear();
    gNumSectors = 0;
    gpSectors = nullptr;
}

#ifdef __cplusplus
}
#endif
