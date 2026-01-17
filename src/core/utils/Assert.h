#pragma once
#include <cstdlib>  // std::abort

#include "core/utils/Log.h"

namespace Hummingbird::Core {

[[noreturn]] inline void assert_failed(const char* expr, const char* file, int line) {
    HB_LOG_ERROR("[assert] failed: " << expr << " (" << file << ":" << line << ")");
    std::abort();
}

}  // namespace Hummingbird::Core

#ifndef HB_ENABLE_ASSERTS
#if !defined(NDEBUG)
#define HB_ENABLE_ASSERTS 1
#else
#define HB_ENABLE_ASSERTS 0
#endif
#endif

#if HB_ENABLE_ASSERTS
#define HB_ASSERT(expr)                                                      \
    do {                                                                     \
        if (!(expr)) {                                                       \
            ::Hummingbird::Core::assert_failed(#expr, __FILE__, __LINE__);   \
        }                                                                    \
    } while (0)
#else
#define HB_ASSERT(expr)     \
    do {                    \
        (void)sizeof(expr); \
    } while (0)
#endif
