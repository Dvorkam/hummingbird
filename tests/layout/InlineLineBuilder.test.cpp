#include <gtest/gtest.h>

#include "layout/InlineLineBuilder.h"

using Hummingbird::Layout::InlineLineBuilder;
using Hummingbird::Layout::InlineRun;

TEST(InlineLineBuilderTest, WrapsRunsAcrossLines) {
    InlineLineBuilder builder;
    builder.add_run({6.0f, 10.0f});
    builder.add_run({6.0f, 10.0f});
    builder.add_run({4.0f, 12.0f});

    auto fragments = builder.layout(10.0f);
    ASSERT_EQ(fragments.size(), 3u);

    EXPECT_EQ(fragments[0].line_index, 0u);
    EXPECT_FLOAT_EQ(fragments[0].rect.x, 0.0f);
    EXPECT_FLOAT_EQ(fragments[0].rect.y, 0.0f);

    EXPECT_EQ(fragments[1].line_index, 1u);
    EXPECT_FLOAT_EQ(fragments[1].rect.x, 0.0f);
    EXPECT_FLOAT_EQ(fragments[1].rect.y, 10.0f);

    EXPECT_EQ(fragments[2].line_index, 1u);
    EXPECT_FLOAT_EQ(fragments[2].rect.x, 6.0f);
    EXPECT_FLOAT_EQ(fragments[2].rect.y, 10.0f);

    const auto& heights = builder.line_heights();
    ASSERT_EQ(heights.size(), 2u);
    EXPECT_FLOAT_EQ(heights[0], 10.0f);
    EXPECT_FLOAT_EQ(heights[1], 12.0f);
}
