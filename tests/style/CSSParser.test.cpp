#include <gtest/gtest.h>
#include "style/Parser.h"

using namespace Hummingbird::Css;

TEST(CSSParserTest, ParsesSingleRule) {
    Parser parser("div { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.selectors.size(), 1u);
    EXPECT_EQ(rule.selectors[0].type, SelectorType::Tag);
    EXPECT_EQ(rule.selectors[0].value, "div");
    ASSERT_EQ(rule.declarations.size(), 1u);
    EXPECT_EQ(rule.declarations[0].property, "color");
    EXPECT_EQ(rule.declarations[0].value, "red");
}

TEST(CSSParserTest, ParsesSelectorList) {
    Parser parser("h1, h2, .title { margin: 10px; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.selectors.size(), 3u);
    EXPECT_EQ(rule.selectors[0].type, SelectorType::Tag);
    EXPECT_EQ(rule.selectors[0].value, "h1");
    EXPECT_EQ(rule.selectors[1].type, SelectorType::Tag);
    EXPECT_EQ(rule.selectors[1].value, "h2");
    EXPECT_EQ(rule.selectors[2].type, SelectorType::Class);
    EXPECT_EQ(rule.selectors[2].value, "title");
    ASSERT_EQ(rule.declarations.size(), 1u);
    EXPECT_EQ(rule.declarations[0].property, "margin");
    EXPECT_EQ(rule.declarations[0].value, "10px");
}
