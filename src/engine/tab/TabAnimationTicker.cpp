#include "engine/tab/TabAnimationTicker.h"

#include "core/utils/Timing.h"

namespace Hummingbird::Engine {

std::optional<int> TabAnimationTicker::consume_ready_delta_ms(int min_interval_ms) {
    const auto now = Core::Clock::now();
    if (!has_tick_) {
        last_tick_ = now;
        has_tick_ = true;
        return std::nullopt;
    }

    int delta_ms = static_cast<int>(Core::duration_ms(last_tick_, now));
    last_tick_ = now;
    if (delta_ms > 0) {
        accumulator_ms_ += delta_ms;
    }
    if (accumulator_ms_ < min_interval_ms) {
        return std::nullopt;
    }

    int ready_delta_ms = accumulator_ms_;
    accumulator_ms_ = 0;
    return ready_delta_ms;
}

void TabAnimationTicker::reset() {
    has_tick_ = false;
    accumulator_ms_ = 0;
}

}  // namespace Hummingbird::Engine
