#include "core/utils/Log.h"

#include <gtest/gtest.h>

#include <sstream>

TEST(LogTest, EmitsWhenEnabled) {
    std::stringstream buffer;
    auto* old = std::cerr.rdbuf(buffer.rdbuf());

    HB_LOG_INFO("hello");
    HB_LOG_WARN("world");

    std::cerr.rdbuf(old);
    auto output = buffer.str();

#if HB_LOG_LEVEL >= 3
    EXPECT_NE(output.find("[info] hello"), std::string::npos);
    EXPECT_NE(output.find("[warn] world"), std::string::npos);
#elif HB_LOG_LEVEL >= 2
    EXPECT_EQ(output.find("[info] hello"), std::string::npos);
    EXPECT_NE(output.find("[warn] world"), std::string::npos);
#elif HB_LOG_LEVEL >= 1
    EXPECT_EQ(output.find("[info] hello"), std::string::npos);
    EXPECT_EQ(output.find("[warn] world"), std::string::npos);
#else
    EXPECT_TRUE(output.empty());
#endif
}
