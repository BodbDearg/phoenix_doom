#include "burger.h"
#include "Endian.h"

/**********************************

    Return the pointer of a shape from a shape array

**********************************/

const void* GetShapeIndexPtr(const void* ShapeArrayPtr, Word Index)
{
    const uint32_t* const pShapeArrayOffsets = (const uint32_t*) ShapeArrayPtr;
    const uint32_t shapeArrayOffset = byteSwappedU32(pShapeArrayOffsets[Index]);
    return &((Byte*) ShapeArrayPtr)[shapeArrayOffset];
}
