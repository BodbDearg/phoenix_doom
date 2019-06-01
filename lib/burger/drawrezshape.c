#include "burger.h"

#include "DoomResources.h"

//---------------------------------------------------------------------------------------------------------------------
// Draw a shape using a resource number
//---------------------------------------------------------------------------------------------------------------------
void DrawRezShape(Word x, Word y, Word RezNum) {
    DrawShape(x, y, loadDoomResourceData(RezNum));
    releaseDoomResource(RezNum);
}
