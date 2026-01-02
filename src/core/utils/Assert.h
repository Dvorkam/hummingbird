#pragma once
#include <cstdlib>  // std::abort

#include "core/utils/Log.h"

#ifndef HB_ENABLE_ASSERTS
#if !defined(NDEBUG)
#define HB_ENABLE_ASSERTS 1
#else
#define HB_ENABLE_ASSERTS 0
#endif
#endif

#if HB_ENABLE_ASSERTS
#define HB_ASSERT(expr)                      \
    do {                                     \
        if (!(expr)) {                       \
            HB_LOG_ERROR("ASSERT FAILED: "); \
            std::abort();                    \
        }                                    \
    } while (0)
#else
#define HB_ASSERT(expr)     \
    do {                    \
        (void)sizeof(expr); \
    } while (0)
#endif