#pragma once

#include <chrono>
#include <optional>

namespace Hummingbird::Engine {

class TabAnimationTicker {
public:
    std::optional<int> consume_ready_delta_ms(int min_interval_ms);
    void reset();

private:
    std::chrono::steady_clock::time_point last_tick_{};
    bool has_tick_ = false;
    int accumulator_ms_ = 0;
};

}  // namespace Hummingbird::Engine
