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

TEST(CSSParserTest, ParsesPseudoClassSelector) {
    Parser parser("input:focus, button:hover, button:active { color: red; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& selectors = sheet.rules[0].selectors;
    ASSERT_EQ(selectors.size(), 3u);

    ASSERT_EQ(selectors[0].parts.size(), 1u);
    EXPECT_EQ(selectors[0].parts[0].tag, "input");
    ASSERT_EQ(selectors[0].parts[0].pseudo_classes.size(), 1u);
    EXPECT_EQ(selectors[0].parts[0].pseudo_classes[0], SelectorPart::PseudoClass::Focus);

    ASSERT_EQ(selectors[1].parts.size(), 1u);
    EXPECT_EQ(selectors[1].parts[0].tag, "button");
    ASSERT_EQ(selectors[1].parts[0].pseudo_classes.size(), 1u);
    EXPECT_EQ(selectors[1].parts[0].pseudo_classes[0], SelectorPart::PseudoClass::Hover);

    ASSERT_EQ(selectors[2].parts.size(), 1u);
    EXPECT_EQ(selectors[2].parts[0].tag, "button");
    ASSERT_EQ(selectors[2].parts[0].pseudo_classes.size(), 1u);
    EXPECT_EQ(selectors[2].parts[0].pseudo_classes[0], SelectorPart::PseudoClass::Active);
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

TEST(CSSParserTest, ParsesOverflowProperties) {
    Parser parser("div { overflow: hidden; overflow-y: scroll; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 2u);

    EXPECT_EQ(decls[0].property, Property::Overflow);
    ASSERT_EQ(decls[0].value.type, Value::Type::Identifier);
    EXPECT_EQ(decls[0].value.ident, "hidden");

    EXPECT_EQ(decls[1].property, Property::OverflowY);
    ASSERT_EQ(decls[1].value.type, Value::Type::Identifier);
    EXPECT_EQ(decls[1].value.ident, "scroll");
}

TEST(CSSParserTest, ParsesCursorProperty) {
    Parser parser("a { cursor: pointer; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 1u);

    EXPECT_EQ(decls[0].property, Property::Cursor);
    ASSERT_EQ(decls[0].value.type, Value::Type::Identifier);
    EXPECT_EQ(decls[0].value.ident, "pointer");
}

TEST(CSSParserTest, ParsesVerticalAlignProperty) {
    Parser parser("span { vertical-align: middle; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 1u);

    EXPECT_EQ(decls[0].property, Property::VerticalAlign);
    ASSERT_EQ(decls[0].value.type, Value::Type::Identifier);
    EXPECT_EQ(decls[0].value.ident, "middle");
}

TEST(CSSParserTest, ParsesBoxShadowProperty) {
    Parser parser("div { box-shadow: 2px 4px 6px #000; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 1u);

    EXPECT_EQ(decls[0].property, Property::BoxShadow);
    ASSERT_EQ(decls[0].value.type, Value::Type::Shadow);
    EXPECT_FLOAT_EQ(decls[0].value.shadow.offset_x.value, 2.0f);
    EXPECT_EQ(decls[0].value.shadow.offset_x.unit, Unit::Px);
    EXPECT_FLOAT_EQ(decls[0].value.shadow.offset_y.value, 4.0f);
    EXPECT_EQ(decls[0].value.shadow.offset_y.unit, Unit::Px);
    EXPECT_FLOAT_EQ(decls[0].value.shadow.blur.value, 6.0f);
    EXPECT_EQ(decls[0].value.shadow.blur.unit, Unit::Px);
    EXPECT_EQ(decls[0].value.shadow.color.r, 0);
    EXPECT_EQ(decls[0].value.shadow.color.g, 0);
    EXPECT_EQ(decls[0].value.shadow.color.b, 0);
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

TEST(CSSParserTest, ParsesFontShorthandIntoComponentDeclarations) {
    Parser parser("p { font: italic 700 20px/1.5 Roboto Mono, monospace; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;

    bool saw_style = false;
    bool saw_weight = false;
    bool saw_size = false;
    bool saw_line_height = false;
    bool saw_family = false;
    for (const auto& decl : decls) {
        if (decl.property == Property::FontStyle) {
            ASSERT_EQ(decl.value.type, Value::Type::Identifier);
            EXPECT_EQ(decl.value.ident, "italic");
            saw_style = true;
        }
        if (decl.property == Property::FontWeight) {
            ASSERT_EQ(decl.value.type, Value::Type::Number);
            EXPECT_FLOAT_EQ(decl.value.number, 700.0f);
            saw_weight = true;
        }
        if (decl.property == Property::FontSize) {
            ASSERT_EQ(decl.value.type, Value::Type::Length);
            EXPECT_FLOAT_EQ(decl.value.length.value, 20.0f);
            EXPECT_EQ(decl.value.length.unit, Unit::Px);
            saw_size = true;
        }
        if (decl.property == Property::LineHeight) {
            ASSERT_EQ(decl.value.type, Value::Type::Number);
            EXPECT_FLOAT_EQ(decl.value.number, 1.5f);
            saw_line_height = true;
        }
        if (decl.property == Property::FontFamily) {
            ASSERT_EQ(decl.value.type, Value::Type::Identifier);
            EXPECT_EQ(decl.value.ident, "Roboto Mono monospace");
            saw_family = true;
        }
        EXPECT_NE(decl.property, Property::Font);
    }

    EXPECT_TRUE(saw_style);
    EXPECT_TRUE(saw_weight);
    EXPECT_TRUE(saw_size);
    EXPECT_TRUE(saw_line_height);
    EXPECT_TRUE(saw_family);
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

TEST(CSSParserTest, SkipsAtRuleBlocksWithoutLeakingInnerRules) {
    Parser parser(R"(
        @charset "utf-8";
        @import url(other.css);
        @media only screen and (max-width: 425px) {
            .mobile-a { color: red; }
            .mobile-b { color: blue; }
        }
        @font-face { font-family: X; src: url(x.woff); }
        @supports (display: grid) {
            .grid-only { color: red; }
        }
        .keep { color: green; }
    )");
    auto sheet = parser.parse();
    // Evaluable @media rules survive WITH their condition; @font-face/@supports
    // blocks are skipped entirely, and nothing leaks unconditioned.
    ASSERT_EQ(sheet.rules.size(), 3u);
    ASSERT_TRUE(sheet.rules[0].media.has_value());
    EXPECT_FLOAT_EQ(*sheet.rules[0].media->max_width, 425.0f);
    ASSERT_TRUE(sheet.rules[1].media.has_value());
    EXPECT_FLOAT_EQ(*sheet.rules[1].media->max_width, 425.0f);
    EXPECT_FALSE(sheet.rules[2].media.has_value());
    EXPECT_EQ(sheet.rules[2].selectors[0].parts[0].classes.at(0), "keep");
}

TEST(CSSParserTest, NestedAtRuleBlocksKeepConditions) {
    Parser parser(R"(
        @media screen {
            @media (max-width: 100px) {
                .inner { color: red; }
            }
            .also-inner { color: red; }
        }
        .keep { color: green; }
    )");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 3u);
    // .inner carries the intersected nested condition.
    ASSERT_TRUE(sheet.rules[0].media.has_value());
    ASSERT_TRUE(sheet.rules[0].media->max_width.has_value());
    EXPECT_FLOAT_EQ(*sheet.rules[0].media->max_width, 100.0f);
    // .also-inner sits in the unconstrained "screen" block (always matches).
    ASSERT_TRUE(sheet.rules[1].media.has_value());
    EXPECT_FALSE(sheet.rules[1].media->max_width.has_value());
    EXPECT_FALSE(sheet.rules[2].media.has_value());
    EXPECT_EQ(sheet.rules[2].selectors[0].parts[0].classes.at(0), "keep");
}

TEST(CSSParserTest, ParsesWidthMediaConditionsOntoRules) {
    Parser parser(R"(
        @media only screen and (min-width: 864px) and (max-height: 361.25px) {
            .desktop { color: red; }
        }
        .always { color: green; }
    )");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 2u);
    const auto& conditioned = sheet.rules[0];
    ASSERT_TRUE(conditioned.media.has_value());
    ASSERT_TRUE(conditioned.media->min_width.has_value());
    EXPECT_FLOAT_EQ(*conditioned.media->min_width, 864.0f);
    ASSERT_TRUE(conditioned.media->max_height.has_value());
    EXPECT_FLOAT_EQ(*conditioned.media->max_height, 361.25f);
    EXPECT_FALSE(conditioned.media->max_width.has_value());
    EXPECT_FALSE(sheet.rules[1].media.has_value());
}

TEST(CSSParserTest, SkipsUnevaluableMediaBlocks) {
    Parser parser(R"(
        @media print { .print-only { color: red; } }
        @media (prefers-color-scheme: dark) { .dark-only { color: red; } }
        @media screen and (max-width: 40em) { .em-based { color: red; } }
        @media screen, (min-width: 100px) { .comma-list { color: red; } }
        .keep { color: green; }
    )");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    EXPECT_EQ(sheet.rules[0].selectors[0].parts[0].classes.at(0), "keep");
}

TEST(CSSParserTest, NestedMediaBlocksIntersectConditions) {
    Parser parser(R"(
        @media (min-width: 600px) {
            @media (max-width: 900px) {
                .band { color: red; }
            }
        }
    )");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    ASSERT_TRUE(sheet.rules[0].media.has_value());
    ASSERT_TRUE(sheet.rules[0].media->min_width.has_value());
    EXPECT_FLOAT_EQ(*sheet.rules[0].media->min_width, 600.0f);
    ASSERT_TRUE(sheet.rules[0].media->max_width.has_value());
    EXPECT_FLOAT_EQ(*sheet.rules[0].media->max_width, 900.0f);
}

TEST(CSSParserTest, ParsesRgbAndRgbaColorFunctions) {
    Parser parser(R"(
        .a { color: rgb(10, 20, 30); }
        .b { border: 1px solid rgba(0, 0, 0, .15); }
        .c { background-color: rgba(137.5, 137.5, 137.5, .9); }
        .d { color: rgba(50%, 100%, 0%, 50%); }
    )");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 4u);

    const auto& a = sheet.rules[0].declarations;
    ASSERT_EQ(a.size(), 1u);
    EXPECT_EQ(a[0].value.type, Value::Type::Color);
    EXPECT_EQ(a[0].value.color.r, 10);
    EXPECT_EQ(a[0].value.color.g, 20);
    EXPECT_EQ(a[0].value.color.b, 30);
    EXPECT_EQ(a[0].value.color.a, 255);

    // border shorthand: width + style + color; rgba alpha 0.15 -> 38.
    bool found_border_color = false;
    for (const auto& decl : sheet.rules[1].declarations) {
        if (decl.property == Property::BorderColor) {
            found_border_color = true;
            EXPECT_EQ(decl.value.color.r, 0);
            EXPECT_EQ(decl.value.color.a, 38);
        }
    }
    EXPECT_TRUE(found_border_color);

    const auto& c = sheet.rules[2].declarations;
    ASSERT_EQ(c.size(), 1u);
    EXPECT_EQ(c[0].value.type, Value::Type::Color);
    EXPECT_EQ(c[0].value.color.r, 138);  // 137.5 rounds up
    EXPECT_EQ(c[0].value.color.a, 230);  // 0.9 * 255

    const auto& d = sheet.rules[3].declarations;
    ASSERT_EQ(d.size(), 1u);
    EXPECT_EQ(d[0].value.color.r, 128);  // 50%
    EXPECT_EQ(d[0].value.color.g, 255);
    EXPECT_EQ(d[0].value.color.a, 128);  // 50% alpha
}

TEST(CSSParserTest, BorderRadiusShorthandExpandsToFourCorners) {
    // DDG search-button shape: `0 4px 4px 0` -> TL, TR, BR, BL.
    Parser parser(".a { border-radius: 0 4px 4px 0; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 4u);
    EXPECT_EQ(decls[0].property, Property::BorderTopLeftRadius);
    EXPECT_FLOAT_EQ(decls[0].value.length.value, 0.0f);
    EXPECT_EQ(decls[1].property, Property::BorderTopRightRadius);
    EXPECT_FLOAT_EQ(decls[1].value.length.value, 4.0f);
    EXPECT_EQ(decls[2].property, Property::BorderBottomRightRadius);
    EXPECT_FLOAT_EQ(decls[2].value.length.value, 4.0f);
    EXPECT_EQ(decls[3].property, Property::BorderBottomLeftRadius);
    EXPECT_FLOAT_EQ(decls[3].value.length.value, 0.0f);
}

TEST(CSSParserTest, VendorPrefixedBorderRadiusResolvesToStandard) {
    Parser parser(".a { -webkit-border-radius: 8px; -moz-border-radius-topleft: 3px; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    // -webkit-border-radius expands to four corners, -moz corner longhand to one.
    ASSERT_EQ(decls.size(), 5u);
    EXPECT_EQ(decls[0].property, Property::BorderTopLeftRadius);
    EXPECT_FLOAT_EQ(decls[0].value.length.value, 8.0f);
    EXPECT_EQ(decls[4].property, Property::BorderTopLeftRadius);
    EXPECT_FLOAT_EQ(decls[4].value.length.value, 3.0f);
}

TEST(CSSParserTest, BorderNoneRemovesBorder) {
    Parser parser(".a { border: none; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 2u);
    EXPECT_EQ(decls[0].property, Property::BorderWidth);
    EXPECT_EQ(decls[0].value.type, Value::Type::Length);
    EXPECT_FLOAT_EQ(decls[0].value.length.value, 0.0f);
    EXPECT_EQ(decls[1].property, Property::BorderStyle);
    EXPECT_EQ(decls[1].value.ident, "none");
}

TEST(CSSParserTest, BackgroundNoneClearsColorAndImage) {
    Parser parser(".a { background: none; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 2u);
    EXPECT_EQ(decls[0].property, Property::BackgroundColor);
    EXPECT_EQ(decls[0].value.ident, "none");
    EXPECT_EQ(decls[1].property, Property::BackgroundImage);
    EXPECT_EQ(decls[1].value.ident, "none");
}
