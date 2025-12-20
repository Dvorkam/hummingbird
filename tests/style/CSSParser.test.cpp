#include <gtest/gtest.h>
#include "style/Parser.h"

using namespace Hummingbird::Css;

TEST(CSSParserTest, ParsesSingleRule) {
    Parser parser("div { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Tag);
    EXPECT_EQ(rule.selector.value, "div");
    ASSERT_EQ(rule.declarations.size(), 1u);
    EXPECT_EQ(rule.declarations[0].property, "color");
    EXPECT_EQ(rule.declarations[0].value, "red");
}
