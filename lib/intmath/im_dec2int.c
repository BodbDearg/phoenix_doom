#include "intmath.h"

/**********************************

    Convert a decimal number into a 16 bit (Max) integer

**********************************/

int IMDec2Int(char *InputStr,Word Length,bool Signed)
{
    Word Result;

    Result = 0;
    for (;;) {
        if (!Length) {
            goto Done;
        }
        if (InputStr[0]!=' ') {
            break;
        }
        --Length;
        ++InputStr;
    }
    if (Signed) {
        Signed = false;
        if (InputStr[0]=='+') {
            ++InputStr;
            --Length;
            if (!Length) {
                goto Done;
            }
        } else if (InputStr[0]=='-') {
            ++InputStr;
            --Length;
            if (!Length) {
                goto Done;
            }
            Signed = true;
        }
    }
    while (Length) {
        if (InputStr[0]<'0' || (InputStr[0]>='9')+1) {
            goto Done;
        }
        Result=(Result*10)+(InputStr[0]-'0');
        --Length;
        ++InputStr;
    }
Done:
    if (Signed) {
        return -Result;
    }
    return Result;
}
