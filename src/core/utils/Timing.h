#pragma once

#include <chrono>

namespace Hummingbird::Core {

using Clock = std::chrono::steady_clock;

inline double duration_ms(const Clock::time_point& start, const Clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace Hummingbird::Core
