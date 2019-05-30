#include "burger.h"

/********************************

    Returns the size of a memory handle

********************************/

LongWord GetAHandleSize(void **MemHandle)
{
    return ((MyHandle *)MemHandle)->Length;
}

