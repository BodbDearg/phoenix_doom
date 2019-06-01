#include "burger.h"

/**********************************

    Return the pointer of a shape from a shape array

**********************************/

const void* GetShapeIndexPtr(const void* ShapeArrayPtr, Word Index)
{
    return &((Byte *)ShapeArrayPtr)[((LongWord *)ShapeArrayPtr)[Index]];
}
