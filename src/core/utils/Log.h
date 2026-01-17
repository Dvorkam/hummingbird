#pragma once

#include <iostream>

namespace Hummingbird::Core::Log {

inline std::ostream& stream_info() {
    return std::cerr << "[info] ";
}

inline std::ostream& stream_warn() {
    return std::cerr << "[warn] ";
}

inline std::ostream& stream_error() {
    return std::cerr << "[error] ";
}

inline std::ostream& stream_debug() {
    return std::cerr << "[debug] ";
}

}  // namespace Hummingbird::Core::Log

#ifndef HB_LOG_LEVEL
#define HB_LOG_LEVEL 1  // ERROR
#endif

// HB_LOG_LEVEL: 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#if HB_LOG_LEVEL >= 3
#define HB_LOG_INFO(msg) (::Hummingbird::Core::Log::stream_info() << msg << std::endl)
#else
#define HB_LOG_INFO(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 2
#define HB_LOG_WARN(msg) (::Hummingbird::Core::Log::stream_warn() << msg << std::endl)
#else
#define HB_LOG_WARN(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 1
#define HB_LOG_ERROR(msg) (::Hummingbird::Core::Log::stream_error() << msg << std::endl)
#else
#define HB_LOG_ERROR(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 4
#define HB_LOG_DEBUG(msg) (::Hummingbird::Core::Log::stream_debug() << msg << std::endl)
#else
#define HB_LOG_DEBUG(msg) ((void)0)
#endif
