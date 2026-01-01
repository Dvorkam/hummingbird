#include "style/CssParser.h"

#include <gtest/gtest.h>

#include "html/HtmlTagNames.h"

using namespace Hummingbird::Css;

TEST(CSSParserTest, ParsesSingleRule) {
    Parser parser("div { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.selectors.size(), 1u);
    EXPECT_EQ(rule.selectors[0].type, SelectorType::Tag);
    EXPECT_EQ(rule.selectors[0].value, Hummingbird::Html::TagNames::Div);
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
    EXPECT_EQ(rule.selectors[0].value, Hummingbird::Html::TagNames::H1);
    EXPECT_EQ(rule.selectors[1].type, SelectorType::Tag);
    EXPECT_EQ(rule.selectors[1].value, Hummingbird::Html::TagNames::H2);
    EXPECT_EQ(rule.selectors[2].type, SelectorType::Class);
    EXPECT_EQ(rule.selectors[2].value, "title");
    ASSERT_EQ(rule.declarations.size(), 1u);
    EXPECT_EQ(rule.declarations[0].property, Property::Margin);
    EXPECT_EQ(rule.declarations[0].value.type, Value::Type::Length);
    EXPECT_FLOAT_EQ(rule.declarations[0].value.length.value, 10.0f);
    EXPECT_EQ(rule.declarations[0].value.length.unit, Unit::Px);
}

TEST(CSSParserTest, ParsesHexColor) {
    Parser parser("div { color: #abc; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.declarations.size(), 1u);
    EXPECT_EQ(rule.declarations[0].property, Property::Color);
    ASSERT_EQ(rule.declarations[0].value.type, Value::Type::Color);
    EXPECT_EQ(rule.declarations[0].value.color.r, 170);
    EXPECT_EQ(rule.declarations[0].value.color.g, 187);
    EXPECT_EQ(rule.declarations[0].value.color.b, 204);
}

TEST(CSSParserTest, ParsesBackgroundColorAndShortHex) {
    Parser parser("div { color: #333; background-color: white; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.declarations.size(), 2u);

    EXPECT_EQ(rule.declarations[0].property, Property::Color);
    ASSERT_EQ(rule.declarations[0].value.type, Value::Type::Color);
    EXPECT_EQ(rule.declarations[0].value.color.r, 51);
    EXPECT_EQ(rule.declarations[0].value.color.g, 51);
    EXPECT_EQ(rule.declarations[0].value.color.b, 51);

    EXPECT_EQ(rule.declarations[1].property, Property::BackgroundColor);
    ASSERT_EQ(rule.declarations[1].value.type, Value::Type::Color);
    EXPECT_EQ(rule.declarations[1].value.color.r, 255);
    EXPECT_EQ(rule.declarations[1].value.color.g, 255);
    EXPECT_EQ(rule.declarations[1].value.color.b, 255);
}
