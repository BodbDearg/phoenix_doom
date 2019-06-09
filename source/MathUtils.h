#pragma once

#include <stdint.h>

// TODO: test
static inline int32_t sfixedMul16_16(int32_t num1, int32_t num2) {
    int64_t result64 = (int64_t) num1 * (int64_t) num2;
    return (int32_t) (result64 >> 16);
}

// TODO: test
static inline int32_t sfixedDiv16_16(int32_t num1, int32_t num2) {
    int64_t result64 = (((int64_t) num1) << 16) / (int64_t) num2;
    return (int32_t) result64;
}

// TODO: test
static inline int32_t floatToSFixed16_16(float num) {
    double doubleVal = (double) num * 65536.0;
    return (int32_t) doubleVal;
}

// TODO: test
static inline float sfixed16_16ToFloat(int32_t num) {
    double doubleVal = (double) num;
    return (float) (doubleVal / 65536.0);
}
