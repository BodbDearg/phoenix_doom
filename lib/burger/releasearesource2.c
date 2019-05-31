#include "burger.h"
#include "Mem.h"

void ReleaseAResource2(Word RezNum,Word Type) {
    MyRezEntry2 *Entry;
    Entry = ScanRezMap(RezNum,Type);    // Scan for the resource
    
    if (Entry) {
        // DC: TODO: this code used to just mark a resource as purgable, then at some point in the future
        // it would be freed. Is this behavior OK or do we need to defer deletion for some reason?
        MEM_FREE_AND_NULL(Entry->MemPtr);
    }
}
