#include "core/utils/CompatibilityWarnings.h"

#include <gtest/gtest.h>

using Hummingbird::Core::Utils::CompatibilityWarnings;

TEST(CompatibilityWarningsTest, CountsRepeatsAndReportsOnlyFirstOccurrence) {
    CompatibilityWarnings warnings;

    EXPECT_TRUE(warnings.record("style.unsupported-font-family", "montserrat"));
    EXPECT_FALSE(warnings.record("style.unsupported-font-family", "montserrat"));
    EXPECT_FALSE(warnings.record("style.unsupported-font-family", "montserrat"));

    EXPECT_EQ(warnings.total_count(), 3u);
    EXPECT_EQ(warnings.unique_count(), 1u);
    auto entries = warnings.ranked();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].count, 3u);
    EXPECT_EQ(entries[0].detail, "montserrat");
}

TEST(CompatibilityWarningsTest, RanksByCountThenFirstOccurrence) {
    CompatibilityWarnings warnings;

    warnings.record("css", "first");
    warnings.record("css", "second");
    warnings.record("css", "second");
    warnings.record("css", "first");
    warnings.record("css", "third");
    warnings.record("css", "third");
    warnings.record("css", "third");

    auto entries = warnings.ranked();
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].detail, "third");
    EXPECT_EQ(entries[1].detail, "first");
    EXPECT_EQ(entries[2].detail, "second");
}

TEST(CompatibilityWarningsTest, ClearStartsANewDocument) {
    CompatibilityWarnings warnings;
    warnings.record("css", "gap");
    warnings.record("css", "gap");

    warnings.clear();

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(warnings.total_count(), 0u);
    EXPECT_TRUE(warnings.record("css", "gap"));
}
