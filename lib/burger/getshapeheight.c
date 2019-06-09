#include "burger.h"
#include "Endian.h"

//--------------------------------------------------------------------------------------------------
// Returns the height of a shape in pixels
//--------------------------------------------------------------------------------------------------
uint32_t GetShapeHeight(const CelControlBlock* const pShape) {
    uint32_t height = byteSwappedU32(pShape->pre0) >> 6;    // Get the VCount bits
    height &= 0x3FF;                                        // Mask off unused bits
    height += 1;                                            // Needed to get the TRUE result
    return height;
}
