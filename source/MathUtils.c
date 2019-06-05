#include "MathUtils.h"

#include <intmath.h>

// DC: TODO: Remove - this is a temp fix for not having this old assembly function
Fixed IMFixDiv(Fixed a, Fixed b) {
    return sfixedDiv16_16(a, b);
}

// DC: TODO: Remove - this is a temp fix for not having this old assembly function
Fixed IMFixMul(Fixed a, Fixed b) {
    return sfixedMul16_16(a, b);
}
