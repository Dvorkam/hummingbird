#include "style/parser/CssParser.h"

#include <gtest/gtest.h>

#include <sstream>

#include "html/HtmlTagNames.h"

using namespace Hummingbird::Css;

TEST(CSSParserTest, ParsesSingleRule) {
    Parser parser("div { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.selectors.size(), 1u);
    ASSERT_EQ(rule.selectors[0].parts.size(), 1u);
    const auto& part = rule.selectors[0].parts[0];
    EXPECT_EQ(part.tag, Hummingbird::Html::TagNames::Div);
    EXPECT_TRUE(part.id.empty());
    EXPECT_TRUE(part.classes.empty());
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
    ASSERT_EQ(rule.selectors[0].parts.size(), 1u);
    ASSERT_EQ(rule.selectors[1].parts.size(), 1u);
    ASSERT_EQ(rule.selectors[2].parts.size(), 1u);
    EXPECT_EQ(rule.selectors[0].parts[0].tag, Hummingbird::Html::TagNames::H1);
    EXPECT_TRUE(rule.selectors[0].parts[0].classes.empty());
    EXPECT_EQ(rule.selectors[1].parts[0].tag, Hummingbird::Html::TagNames::H2);
    EXPECT_TRUE(rule.selectors[1].parts[0].classes.empty());
    ASSERT_EQ(rule.selectors[2].parts[0].classes.size(), 1u);
    EXPECT_TRUE(rule.selectors[2].parts[0].tag.empty());
    EXPECT_EQ(rule.selectors[2].parts[0].classes[0], "title");
    ASSERT_EQ(rule.declarations.size(), 4u);
    EXPECT_EQ(rule.declarations[0].property, Property::MarginTop);
    EXPECT_EQ(rule.declarations[1].property, Property::MarginRight);
    EXPECT_EQ(rule.declarations[2].property, Property::MarginBottom);
    EXPECT_EQ(rule.declarations[3].property, Property::MarginLeft);
    for (const auto& decl : rule.declarations) {
        EXPECT_EQ(decl.value.type, Value::Type::Length);
        EXPECT_FLOAT_EQ(decl.value.length.value, 10.0f);
        EXPECT_EQ(decl.value.length.unit, Unit::Px);
    }
}

TEST(CSSParserTest, ParsesCompoundSelector) {
    Parser parser("div.note#main { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.selectors.size(), 1u);
    ASSERT_EQ(rule.selectors[0].parts.size(), 1u);
    const auto& part = rule.selectors[0].parts[0];
    EXPECT_EQ(part.tag, Hummingbird::Html::TagNames::Div);
    EXPECT_EQ(part.id, "main");
    ASSERT_EQ(part.classes.size(), 1u);
    EXPECT_EQ(part.classes[0], "note");
}

TEST(CSSParserTest, ParsesUniversalSelector) {
    Parser parser("* { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.selectors.size(), 1u);
    ASSERT_EQ(rule.selectors[0].parts.size(), 1u);
    EXPECT_EQ(rule.selectors[0].parts[0].tag, "*");
}

TEST(CSSParserTest, ParsesDescendantSelector) {
    Parser parser("div .note { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.selectors.size(), 1u);
    ASSERT_EQ(rule.selectors[0].parts.size(), 2u);
    EXPECT_EQ(rule.selectors[0].parts[0].tag, Hummingbird::Html::TagNames::Div);
    ASSERT_EQ(rule.selectors[0].parts[1].classes.size(), 1u);
    EXPECT_EQ(rule.selectors[0].parts[1].classes[0], "note");
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

TEST(CSSParserTest, ParsesFullHexColorWithDigits) {
    Parser parser("div { color: #99cc99; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.declarations.size(), 1u);
    EXPECT_EQ(rule.declarations[0].property, Property::Color);
    ASSERT_EQ(rule.declarations[0].value.type, Value::Type::Color);
    EXPECT_EQ(rule.declarations[0].value.color.r, 0x99);
    EXPECT_EQ(rule.declarations[0].value.color.g, 0xcc);
    EXPECT_EQ(rule.declarations[0].value.color.b, 0x99);
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

TEST(CSSParserTest, ParsesBackgroundImageUrl) {
    Parser parser("div { background-image: url(/img/logo.png); }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 1u);
    EXPECT_EQ(decls[0].property, Property::BackgroundImage);
    ASSERT_EQ(decls[0].value.type, Value::Type::Url);
    EXPECT_EQ(decls[0].value.ident, "/img/logo.png");
}

TEST(CSSParserTest, ExpandsBackgroundShorthandForImages) {
    Parser parser("div { background: url(/img/logo.png) no-repeat center; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    bool has_image = false;
    bool has_repeat = false;
    bool has_position = false;
    for (const auto& decl : decls) {
        if (decl.property == Property::BackgroundImage) {
            has_image = true;
        } else if (decl.property == Property::BackgroundRepeat) {
            has_repeat = true;
        } else if (decl.property == Property::BackgroundPosition) {
            has_position = true;
        }
    }
    EXPECT_TRUE(has_image);
    EXPECT_TRUE(has_repeat);
    EXPECT_TRUE(has_position);
}

TEST(CSSParserTest, DedupesUnsupportedPropertyWarnings) {
    Parser parser("div { bogus: 1; bogus: 2; }");
    std::ostringstream captured;
    auto* old_buf = std::cerr.rdbuf(captured.rdbuf());
    auto sheet = parser.parse();
    std::cerr.rdbuf(old_buf);
    ASSERT_EQ(sheet.rules.size(), 1u);
    const std::string output = captured.str();
    const std::string needle = "Unsupported CSS property encountered: bogus";
    size_t count = 0;
    size_t pos = 0;
    while ((pos = output.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    EXPECT_EQ(count, 1u);
}

TEST(CSSParserTest, RecoversMissingSemicolonBetweenDeclarations) {
    Parser parser("div { color: red background-color: blue; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 2u);
    EXPECT_EQ(decls[0].property, Property::Color);
    EXPECT_EQ(decls[1].property, Property::BackgroundColor);
}

TEST(CSSParserTest, SkipsMalformedDeclarations) {
    Parser parser("div { color red; font-size: ; background-color: blue; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 1u);
    EXPECT_EQ(decls[0].property, Property::BackgroundColor);
}

TEST(CSSParserTest, SkipsMalformedRuleAndContinues) {
    Parser parser("div color: red; p { color: blue; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_EQ(sheet.rules[0].selectors.size(), 1u);
    ASSERT_EQ(sheet.rules[0].selectors[0].parts.size(), 1u);
    EXPECT_EQ(sheet.rules[0].selectors[0].parts[0].tag, Hummingbird::Html::TagNames::P);
    ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
    EXPECT_EQ(sheet.rules[0].declarations[0].property, Property::Color);
}
