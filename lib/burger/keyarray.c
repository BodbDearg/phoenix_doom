#include "burger.h"

/**********************************

    Interrupt driven keyboard scanning array

**********************************/

volatile Byte KeyArray[128];        /* Shift/CapsLock and other flags */
