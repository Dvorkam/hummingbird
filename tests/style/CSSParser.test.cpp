#include "style/parser/CssParser.h"

#include <gtest/gtest.h>

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
    ASSERT_EQ(rule.selectors[0].combinators.size(), 1u);
    EXPECT_EQ(rule.selectors[0].combinators[0], Selector::Combinator::Descendant);
    EXPECT_EQ(rule.selectors[0].parts[0].tag, Hummingbird::Html::TagNames::Div);
    ASSERT_EQ(rule.selectors[0].parts[1].classes.size(), 1u);
    EXPECT_EQ(rule.selectors[0].parts[1].classes[0], "note");
}

TEST(CSSParserTest, ParsesChildSelector) {
    Parser parser("div > .note { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& rule = sheet.rules[0];
    ASSERT_EQ(rule.selectors.size(), 1u);
    ASSERT_EQ(rule.selectors[0].parts.size(), 2u);
    ASSERT_EQ(rule.selectors[0].combinators.size(), 1u);
    EXPECT_EQ(rule.selectors[0].combinators[0], Selector::Combinator::Child);
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

TEST(CSSParserTest, ParsesPercentLengths) {
    Parser parser("div { width: 70%; top: 24%; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 2u);

    EXPECT_EQ(decls[0].property, Property::Width);
    ASSERT_EQ(decls[0].value.type, Value::Type::Length);
    EXPECT_FLOAT_EQ(decls[0].value.length.value, 70.0f);
    EXPECT_EQ(decls[0].value.length.unit, Unit::Percent);

    EXPECT_EQ(decls[1].property, Property::Top);
    ASSERT_EQ(decls[1].value.type, Value::Type::Length);
    EXPECT_FLOAT_EQ(decls[1].value.length.value, 24.0f);
    EXPECT_EQ(decls[1].value.length.unit, Unit::Percent);
}

TEST(CSSParserTest, ParsesLeadingDotAndSignedNumbers) {
    Parser parser("div { padding: .75em; margin-left: -0.35em; right: 2px; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_GE(decls.size(), 6u);

    bool saw_padding_top = false;
    for (const auto& decl : decls) {
        if (decl.property != Property::PaddingTop) {
            continue;
        }
        ASSERT_EQ(decl.value.type, Value::Type::Length);
        EXPECT_FLOAT_EQ(decl.value.length.value, 0.75f);
        EXPECT_EQ(decl.value.length.unit, Unit::Em);
        saw_padding_top = true;
        break;
    }
    EXPECT_TRUE(saw_padding_top);

    bool saw_negative_margin = false;
    bool saw_right_px = false;
    for (const auto& decl : sheet.rules[0].declarations) {
        if (decl.property == Property::MarginLeft) {
            ASSERT_EQ(decl.value.type, Value::Type::Length);
            EXPECT_FLOAT_EQ(decl.value.length.value, -0.35f);
            EXPECT_EQ(decl.value.length.unit, Unit::Em);
            saw_negative_margin = true;
        }
        if (decl.property == Property::Right) {
            ASSERT_EQ(decl.value.type, Value::Type::Length);
            EXPECT_FLOAT_EQ(decl.value.length.value, 2.0f);
            EXPECT_EQ(decl.value.length.unit, Unit::Px);
            saw_right_px = true;
        }
    }
    EXPECT_TRUE(saw_negative_margin);
    EXPECT_TRUE(saw_right_px);
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

TEST(CSSParserTest, ParsesBorderSideShorthandIntoSideWidthDeclaration) {
    Parser parser("div { border-top: 3px solid #123456; border-left: 2px ridge #999; border-bottom: 4px solid #111; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;

    bool saw_top_width = false;
    bool saw_left_width = false;
    bool saw_bottom_width = false;
    bool saw_style = false;
    bool saw_color = false;
    bool saw_global_border_width = false;
    for (const auto& decl : decls) {
        if (decl.property == Property::BorderTopWidth) {
            ASSERT_EQ(decl.value.type, Value::Type::Length);
            EXPECT_FLOAT_EQ(decl.value.length.value, 3.0f);
            EXPECT_EQ(decl.value.length.unit, Unit::Px);
            saw_top_width = true;
        }
        if (decl.property == Property::BorderLeftWidth) {
            ASSERT_EQ(decl.value.type, Value::Type::Length);
            EXPECT_FLOAT_EQ(decl.value.length.value, 2.0f);
            EXPECT_EQ(decl.value.length.unit, Unit::Px);
            saw_left_width = true;
        }
        if (decl.property == Property::BorderBottomWidth) {
            ASSERT_EQ(decl.value.type, Value::Type::Length);
            EXPECT_FLOAT_EQ(decl.value.length.value, 4.0f);
            EXPECT_EQ(decl.value.length.unit, Unit::Px);
            saw_bottom_width = true;
        }
        if (decl.property == Property::BorderWidth) {
            saw_global_border_width = true;
        }
        if (decl.property == Property::BorderStyle) {
            saw_style = true;
        }
        if (decl.property == Property::BorderColor) {
            saw_color = true;
        }
    }

    EXPECT_TRUE(saw_top_width);
    EXPECT_TRUE(saw_left_width);
    EXPECT_TRUE(saw_bottom_width);
    EXPECT_TRUE(saw_style);
    EXPECT_TRUE(saw_color);
    EXPECT_FALSE(saw_global_border_width);
}

TEST(CSSParserTest, ParsesBorderSideWidthsWithoutCollapsingToGlobalBorderWidth) {
    Parser parser("div { border-top: 5px solid #224488; border-right-width: 3px; border-bottom: 2px inset #224488; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;

    bool saw_top = false;
    bool saw_right = false;
    bool saw_bottom = false;
    bool saw_global = false;
    for (const auto& decl : decls) {
        if (decl.property == Property::BorderTopWidth) {
            saw_top = true;
        }
        if (decl.property == Property::BorderRightWidth) {
            saw_right = true;
        }
        if (decl.property == Property::BorderBottomWidth) {
            saw_bottom = true;
        }
        if (decl.property == Property::BorderWidth) {
            saw_global = true;
        }
    }
    EXPECT_TRUE(saw_top);
    EXPECT_TRUE(saw_right);
    EXPECT_TRUE(saw_bottom);
    EXPECT_FALSE(saw_global);
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
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_EQ(sheet.unknown_properties.size(), 1u);
    EXPECT_TRUE(sheet.unknown_properties.count("bogus"));
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
