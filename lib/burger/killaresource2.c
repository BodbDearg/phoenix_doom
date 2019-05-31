#include <burger.h>
#include "Mem.h"

//---------------------------------------------------------------------------------------------------------------------
// Destroy the data associated with a resource
//---------------------------------------------------------------------------------------------------------------------
void KillAResource2(Word RezNum,Word Type)
{
    MyRezEntry2* pEntry = ScanRezMap(RezNum,Type);    // Scan for the resource
    
    if (pEntry) {
        MEM_FREE_AND_NULL(pEntry->MemPtr);
    }
}
