#include "burger.h"
#include "Endian.h"

//--------------------------------------------------------------------------------------------------
// Returns the width of a shape in pixels
//--------------------------------------------------------------------------------------------------
uint32_t GetShapeWidth(const CelControlBlock* const pShape) {
    uint32_t width = byteSwappedU32(pShape->pre1) & 0x7FF;  // Get the HCount bits
    width += 1;                                             // Needed to get the TRUE result
    return width;
}
