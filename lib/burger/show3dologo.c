#include "burger.h"

// DC: 3DO specific headers - remove
#if 0
    #include <Task.h>
    #include <Filefunctions.h>
#endif

#define LOGOPROG "Logo"

/**********************************

    This routine will load an executable program
    that will display the 3DO logo and then fade to black

**********************************/

void Show3DOLogo(void)
{
    // DC: FIXME: Do we even want this anymore?
    #if 0
        Item LogoItem;
        LogoItem=LoadProgram(LOGOPROG);     /* Load and begin execution */
        do {
            Yield();                        /* Yield all CPU time to the other program */
        } while (LookupItem(LogoItem));     /* Wait until the program quits */
        DeleteItem(LogoItem);               /* Dispose of the 3DO logo code */
    #endif
}
