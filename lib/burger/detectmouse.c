#include "burger.h"

/**********************************

    See if a mouse is plugged into the 3DO

**********************************/

bool DetectMouse(void)
{
    MousePresent = true;            /* Just assume it's ok */
    return true;
}
