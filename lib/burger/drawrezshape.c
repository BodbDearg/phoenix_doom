#include "burger.h"
#include "Resources.h"

//---------------------------------------------------------------------------------------------------------------------
// Draw a shape using a resource number
//---------------------------------------------------------------------------------------------------------------------
void DrawRezShape(Word x, Word y, Word RezNum) {
    DrawShape(x, y, loadResourceData(RezNum));
    releaseResource(RezNum);
}
