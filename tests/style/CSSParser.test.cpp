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
    EXPECT_EQ(rule.declarations[0].property, Property::Color);
    EXPECT_EQ(rule.declarations[0].value.type, Value::Type::Color);
    EXPECT_EQ(rule.declarations[0].value.color.r, 255);
    EXPECT_EQ(rule.declarations[0].value.color.g, 0);
    EXPECT_EQ(rule.declarations[0].value.color.b, 0);
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
    EXPECT_EQ(rule.declarations[0].property, Property::Margin);
    EXPECT_EQ(rule.declarations[0].value.type, Value::Type::Length);
    EXPECT_FLOAT_EQ(rule.declarations[0].value.length.value, 10.0f);
    EXPECT_EQ(rule.declarations[0].value.length.unit, Unit::Px);
}
