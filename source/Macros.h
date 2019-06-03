#pragma once

#include <stdlib.h>

// Regular assert without a message
#define ASSERT(Condition)\
    do {\
        if (!(Condition)) {\
            printf("Assert failed! Condition: %s\n", #Condition);\
            abort();\
        }\
    } while (0)

// Assert with a simple message on failure
#define ASSERT_LOG(Condition, Message)\
    do {\
        if (!(Condition)) {\
            printf("Assert failed! Condition: %s\n", #Condition);\
            printf("%s\n", Message);\
            abort();\
        }\
    } while (0)

// Assert with a formatted message on failure
#define ASSERT_LOG_F(Condition, MessageFormat, ...)\
    do {\
        if (!(Condition)) {\
            printf("Assert failed! Condition: %s\n", #Condition);\
            printf(MessageFormat "\n", __VA_ARGS__);\
            abort();\
        }\
    } while (0)

// Raise a fatal error message
#define FATAL_ERROR(Message)\
    do {\
        printf("%s\n", Message);\
        abort();\
    } while (0)

// Raise a formatted Fatal error message
#define FATAL_ERROR_F(MessageFormat, ...)\
    do {\
        printf(MessageFormat "\n", __VA_ARGS__);\
        abort();\
    } while (0)
