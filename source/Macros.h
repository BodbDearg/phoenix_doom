#pragma once

#define ASSERT(Condition)\
    do {\
        assert(Condition);\
    } while (0)

#define FATAL_ERROR(...)\
    do {\
        printf(__VA_ARGS__);\
        exit(1);\
    } while (0)
