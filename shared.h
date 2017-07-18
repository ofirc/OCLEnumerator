#pragma once

#include <tchar.h>

#if 1
#include <stdio.h>
#define OCL_ENUM_TRACE(...) \
    do \
    { \
        _ftprintf(stderr, TEXT("OCL ENUM trace at %hs:%d: "), __FILE__, __LINE__); \
        _ftprintf(stderr, __VA_ARGS__); \
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
