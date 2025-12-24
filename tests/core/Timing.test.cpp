#include "core/utils/Timing.h"

#include <gtest/gtest.h>

TEST(TimingTest, DurationMsComputesExpectedValue) {
    using Hummingbird::Core::Clock;
    using Hummingbird::Core::duration_ms;

    Clock::time_point start = Clock::time_point(std::chrono::milliseconds(10));
    Clock::time_point end = Clock::time_point(std::chrono::milliseconds(42));

    EXPECT_DOUBLE_EQ(duration_ms(start, end), 32.0);
}
