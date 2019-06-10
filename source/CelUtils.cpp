#include "CelUtils.h"

#include "Endian.h"

extern "C" {

uint32_t getCCBWidth(const CelControlBlock* const pShape) {
    // DC: this logic comes from the original 3DO Doom source.
    // It can be found in the burgerlib function 'GetShapeWidth()':
    uint32_t width = byteSwappedU32(pShape->pre1) & 0x7FF;              // Get the HCount bits
    width += 1;                                                         // Needed to get the TRUE result
    return width;
}

uint32_t getCCBHeight(const CelControlBlock* const pShape) {
    // DC: this logic comes from the original 3DO Doom source.
    // It be found in the burgerlib function 'GetShapeHeight()':
    uint32_t height = byteSwappedU32(pShape->pre0) >> 6;                // Get the VCount bits
    height &= 0x3FF;                                                    // Mask off unused bits
    height += 1;                                                        // Needed to get the TRUE result
    return height;
}

}
