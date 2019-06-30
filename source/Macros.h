#pragma once

// Whether or not asserts are enabled
#ifndef NDEBUG
    #define ASSERTS_ENABLED 1
#endif

// Required includes for error handling
#include <cstdio>
#include <cstdlib>

// Regular assert without a message
#if ASSERTS_ENABLED == 1
    #define ASSERT(Condition)\
        do {\
            if (!(Condition)) {\
                printf("Assert failed! Condition: %s\n", #Condition);\
                abort();\
            }\
        } while (0)
#else
    #define ASSERT(Condition)
#endif

// Assert with a simple message on failure
#if ASSERTS_ENABLED == 1
    #define ASSERT_LOG(Condition, Message)\
        do {\
            if (!(Condition)) {\
                printf("Assert failed! Condition: %s\n", #Condition);\
                printf("%s\n", Message);\
                abort();\
            }\
        } while (0)
#else
    #define ASSERT_LOG(Condition, Message)
#endif

// Assert with a formatted message on failure
#if ASSERTS_ENABLED == 1
    #define ASSERT_LOG_F(Condition, MessageFormat, ...)\
        do {\
            if (!(Condition)) {\
                printf("Assert failed! Condition: %s\n", #Condition);\
                printf(MessageFormat "\n", __VA_ARGS__);\
                abort();\
            }\
        } while (0)
#else
    #define ASSERT_LOG_F(Condition, MessageFormat, ...)
#endif

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

// Used to decorate exception throwing C++ functions
#define THROWS noexcept(false)
