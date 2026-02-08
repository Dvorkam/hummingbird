#include "core/utils/WarnOnce.h"

#include <gtest/gtest.h>

TEST(WarnOnceTest, LogsOncePerKey) {
    Hummingbird::Core::Utils::WarnOnce warn;
    EXPECT_TRUE(warn.should_log("alpha"));
    EXPECT_FALSE(warn.should_log("alpha"));
    EXPECT_TRUE(warn.should_log("beta"));

    warn.clear();
    EXPECT_TRUE(warn.should_log("alpha"));
}
