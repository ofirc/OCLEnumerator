#pragma once

#if 1
#include <stdio.h>
#define OCL_ENUM_TRACE(...) \
    do \
    { \
        fprintf(stderr, "OCL ENUM trace at %s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

#define OCL_ENUM_ASSERT(x) \
    do \
    { \
        if (!(x)) \
        { \
            fprintf(stderr, "OCL ENUM assert at %s:%d: %s failed", __FILE__, __LINE__, #x); \
        } \
    } while (0)
#else
#define OCL_ENUM_TRACE(...)
#define OCL_ENUM_ASSERT(x)
#endif
