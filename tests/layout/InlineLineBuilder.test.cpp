#include <gtest/gtest.h>

#include "layout/InlineLineBuilder.h"

using Hummingbird::Layout::InlineLineBuilder;
using Hummingbird::Layout::InlineRun;

TEST(InlineLineBuilderTest, WrapsRunsAcrossLines) {
    InlineLineBuilder builder;
    builder.add_run({nullptr, 0, "", 6.0f, 10.0f});
    builder.add_run({nullptr, 0, "", 6.0f, 10.0f});
    builder.add_run({nullptr, 0, "", 4.0f, 12.0f});

    auto lines = builder.layout(10.0f);
    ASSERT_EQ(lines.size(), 2u);
    ASSERT_EQ(lines[0].fragments.size(), 1u);
    ASSERT_EQ(lines[1].fragments.size(), 2u);

    EXPECT_EQ(lines[0].fragments[0].line_index, 0u);
    EXPECT_FLOAT_EQ(lines[0].fragments[0].rect.x, 0.0f);
    EXPECT_FLOAT_EQ(lines[0].fragments[0].rect.y, 0.0f);
    EXPECT_FLOAT_EQ(lines[0].height, 10.0f);

    EXPECT_EQ(lines[1].fragments[0].line_index, 1u);
    EXPECT_FLOAT_EQ(lines[1].fragments[0].rect.x, 0.0f);
    EXPECT_FLOAT_EQ(lines[1].fragments[0].rect.y, 10.0f);
    EXPECT_EQ(lines[1].fragments[1].line_index, 1u);
    EXPECT_FLOAT_EQ(lines[1].fragments[1].rect.x, 6.0f);
    EXPECT_FLOAT_EQ(lines[1].fragments[1].rect.y, 10.0f);
    EXPECT_FLOAT_EQ(lines[1].height, 12.0f);
}

TEST(InlineLineBuilderTest, HonorsStartOffset) {
    InlineLineBuilder builder;
    builder.add_run({nullptr, 0, "", 6.0f, 10.0f});
    builder.add_run({nullptr, 0, "", 6.0f, 10.0f});

    auto lines = builder.layout(10.0f, 4.0f);
    ASSERT_EQ(lines.size(), 2u);
    ASSERT_EQ(lines[0].fragments.size(), 1u);
    ASSERT_EQ(lines[1].fragments.size(), 1u);

    EXPECT_EQ(lines[0].fragments[0].line_index, 0u);
    EXPECT_FLOAT_EQ(lines[0].fragments[0].rect.x, 4.0f);

    EXPECT_EQ(lines[1].fragments[0].line_index, 1u);
    EXPECT_FLOAT_EQ(lines[1].fragments[0].rect.x, 0.0f);
    EXPECT_FLOAT_EQ(lines[1].fragments[0].rect.y, 10.0f);
}
