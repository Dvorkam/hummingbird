#include "layout/flow/TextStyleUtils.h"

#include <gtest/gtest.h>

#include "style/types/ComputedStyle.h"

using Hummingbird::Css::ComputedStyle;
using Hummingbird::Layout::TextStyleUtils::resolve_text_font_path;

TEST(TextStyleUtilsTest, ResolvesRobotoMonoForMonospaceFamily) {
    ComputedStyle style;
    style.font_face = "monospace";
    auto path = resolve_text_font_path(&style);
    EXPECT_NE(path.find("RobotoMono-"), std::string::npos);
}

TEST(TextStyleUtilsTest, ResolvesRobotoMonoWhenMonospaceFlagSet) {
    ComputedStyle style;
    style.font_monospace = true;
    auto path = resolve_text_font_path(&style);
    EXPECT_NE(path.find("RobotoMono-"), std::string::npos);
}

TEST(TextStyleUtilsTest, ResolvesRobotoForSansFamily) {
    ComputedStyle style;
    style.font_face = "sans-serif";
    auto path = resolve_text_font_path(&style);
    EXPECT_NE(path.find("Roboto-"), std::string::npos);
    EXPECT_EQ(path.find("RobotoMono-"), std::string::npos);
}

TEST(TextStyleUtilsTest, ResolvesRobotoMonoForShorthandFamilyListWithoutCommaToken) {
    ComputedStyle style;
    style.font_face = "Roboto Mono monospace";
    auto path = resolve_text_font_path(&style);
    EXPECT_NE(path.find("RobotoMono-"), std::string::npos);
}

TEST(TextStyleUtilsTest, ResolvesRobotoMonoForLowercaseCollapsedFamilyList) {
    ComputedStyle style;
    style.font_face = "roboto mono monospace";
    auto path = resolve_text_font_path(&style);
    EXPECT_NE(path.find("RobotoMono-"), std::string::npos);
}

TEST(TextStyleUtilsTest, ResolvesRobotoMonoForQuotedFamilyNames) {
    ComputedStyle style;
    style.font_face = "\"roboto mono\", monospace";
    auto path = resolve_text_font_path(&style);
    EXPECT_NE(path.find("RobotoMono-"), std::string::npos);
}
