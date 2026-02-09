#include "style/compute/StyleEngine.h"

#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "style/compute/StylesheetSource.h"
#include "style/parser/CssParser.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;
namespace Attr = Hummingbird::Html::AttributeNames;

TEST(StyleEngineTest, AppliesRulesAndCascade) {
    // DOM: <div class="box" id="main"><span></span></div>
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "box");
    root->set_attribute(Attr::Id, "main");
    root->append_child(DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span));

    // CSS: tag rule then id rule overriding width
    std::string css = R"(div { width: 50px; margin: 5px; } #main { width: 80px; padding: 3px; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style_root = root->get_computed_style();
    ASSERT_TRUE(style_root);
    EXPECT_EQ(style_root->margin.top, 5);
    EXPECT_EQ(style_root->padding.top, 3);
    ASSERT_TRUE(style_root->width.has_value());
    EXPECT_FLOAT_EQ(style_root->width.value(), 80);

    // Child span should at least have a computed style object (even if empty).
    auto style_child = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(style_child);
}

TEST(StyleEngineTest, AppliesDefaultStylesForUlPreAndAnchor) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto ul = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Ul);
    auto pre = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Pre);
    auto anchor = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);
    auto code = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Code);
    auto blockquote = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Blockquote);
    auto hr = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Hr);
    auto h1 = DomFactory::create_element(arena, Hummingbird::Html::TagNames::H1);

    // Build a small DOM tree to traverse.
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->append_child(std::move(ul));
    root->append_child(std::move(pre));
    root->append_child(std::move(anchor));
    root->append_child(std::move(code));
    root->append_child(std::move(blockquote));
    root->append_child(std::move(hr));
    root->append_child(std::move(h1));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, root.get());

    auto ul_style = dynamic_cast<Element*>(root->get_children()[0].get())->get_computed_style();
    auto pre_style = dynamic_cast<Element*>(root->get_children()[1].get())->get_computed_style();
    auto a_style = dynamic_cast<Element*>(root->get_children()[2].get())->get_computed_style();

    auto code_style = dynamic_cast<Element*>(root->get_children()[3].get())->get_computed_style();
    auto blockquote_style = dynamic_cast<Element*>(root->get_children()[4].get())->get_computed_style();
    auto hr_style = dynamic_cast<Element*>(root->get_children()[5].get())->get_computed_style();
    auto h1_style = dynamic_cast<Element*>(root->get_children()[6].get())->get_computed_style();

    ASSERT_TRUE(ul_style);
    ASSERT_TRUE(pre_style);
    ASSERT_TRUE(a_style);
    ASSERT_TRUE(code_style);
    ASSERT_TRUE(blockquote_style);
    ASSERT_TRUE(hr_style);
    ASSERT_TRUE(h1_style);

    EXPECT_FLOAT_EQ(ul_style->padding.left, 20.0f);
    EXPECT_EQ(pre_style->whitespace, ComputedStyle::WhiteSpace::Preserve);
    EXPECT_TRUE(pre_style->font_monospace);

    EXPECT_EQ(a_style->color.r, 0);
    EXPECT_EQ(a_style->color.g, 0);
    EXPECT_EQ(a_style->color.b, 255);
    EXPECT_TRUE(a_style->underline);

    EXPECT_TRUE(code_style->font_monospace);
    EXPECT_FALSE(code_style->background.has_value());
    EXPECT_GT(code_style->padding.left, 0.0f);

    EXPECT_FLOAT_EQ(blockquote_style->margin.left, 40.0f);
    EXPECT_TRUE(hr_style->height.has_value());
    EXPECT_GT(hr_style->height.value(), 0.0f);

    EXPECT_GT(h1_style->font_size, 16.0f);
    EXPECT_EQ(h1_style->weight, ComputedStyle::FontWeight::Bold);
}

TEST(StyleEngineTest, SubmitInputUsesCompactDefaultWidth) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto submit = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    submit->set_attribute(Attr::Type, "submit");
    auto text = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    text->set_attribute(Attr::Type, "text");

    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->append_child(std::move(submit));
    root->append_child(std::move(text));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, root.get());

    auto submit_style = dynamic_cast<Element*>(root->get_children()[0].get())->get_computed_style();
    auto text_style = dynamic_cast<Element*>(root->get_children()[1].get())->get_computed_style();
    ASSERT_TRUE(submit_style);
    ASSERT_TRUE(text_style);
    EXPECT_TRUE(submit_style->width.has_value());
    EXPECT_FLOAT_EQ(*submit_style->width, 80.0f);
    EXPECT_TRUE(text_style->width.has_value());
    EXPECT_FLOAT_EQ(*text_style->width, 180.0f);
}

TEST(StyleEngineTest, StoresAndInheritsCustomProperties) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    auto grandchild = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);

    child->append_child(std::move(grandchild));
    root->append_child(std::move(child));

    std::string css = R"(div { --brand: #123; } span { --accent: 10px; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto root_style = dynamic_cast<Element*>(root.get())->get_computed_style();
    auto child_style = dynamic_cast<Element*>(root->get_children()[0].get())->get_computed_style();
    auto grandchild_style =
        dynamic_cast<Element*>(root->get_children()[0]->get_children()[0].get())->get_computed_style();

    ASSERT_TRUE(root_style);
    ASSERT_TRUE(child_style);
    ASSERT_TRUE(grandchild_style);

    auto root_brand = root_style->custom_properties.find("--brand");
    ASSERT_NE(root_brand, root_style->custom_properties.end());
    EXPECT_EQ(root_brand->second, "#123");

    auto child_brand = child_style->custom_properties.find("--brand");
    ASSERT_NE(child_brand, child_style->custom_properties.end());
    EXPECT_EQ(child_brand->second, "#123");

    auto grandchild_brand = grandchild_style->custom_properties.find("--brand");
    ASSERT_NE(grandchild_brand, grandchild_style->custom_properties.end());
    EXPECT_EQ(grandchild_brand->second, "#123");

    auto grandchild_accent = grandchild_style->custom_properties.find("--accent");
    ASSERT_NE(grandchild_accent, grandchild_style->custom_properties.end());
    EXPECT_EQ(grandchild_accent->second, "10px");
}

TEST(StyleEngineTest, ResolvesVarForColors) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto color_node = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    auto fallback_node = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);

    root->append_child(std::move(color_node));
    root->append_child(std::move(fallback_node));

    std::string css = R"(div { --brand: #123; } p { color: var(--brand); }
                          span { background-color: var(--missing, #ff0000); })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto color_style = root->get_children()[0]->get_computed_style();
    auto fallback_style = root->get_children()[1]->get_computed_style();

    ASSERT_TRUE(color_style);
    ASSERT_TRUE(fallback_style);

    EXPECT_EQ(color_style->color.r, 17);
    EXPECT_EQ(color_style->color.g, 34);
    EXPECT_EQ(color_style->color.b, 51);

    ASSERT_TRUE(fallback_style->background.has_value());
    EXPECT_EQ(fallback_style->background->r, 255);
    EXPECT_EQ(fallback_style->background->g, 0);
    EXPECT_EQ(fallback_style->background->b, 0);
}

TEST(StyleEngineTest, AppliesFloatProperty) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "floaty");

    std::string css = R"(.floaty { float: right; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->float_type, ComputedStyle::Float::Right);
}

TEST(StyleEngineTest, AppliesBoxSizingProperty) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "boxed");

    std::string css = R"(.boxed { box-sizing: border-box; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->box_sizing, ComputedStyle::BoxSizing::BorderBox);
}

TEST(StyleEngineTest, PreservesPercentUnitsForLayoutProperties) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Id, "box");

    std::string css = R"(#box { width: 70%; top: 24%; left: 10%; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->width.has_value());
    EXPECT_FLOAT_EQ(*style->width, 70.0f);
    EXPECT_TRUE(style->width_is_percent);
    ASSERT_TRUE(style->top.has_value());
    EXPECT_FLOAT_EQ(*style->top, 24.0f);
    EXPECT_TRUE(style->top_is_percent);
    ASSERT_TRUE(style->left.has_value());
    EXPECT_FLOAT_EQ(*style->left, 10.0f);
    EXPECT_TRUE(style->left_is_percent);
}

TEST(StyleEngineTest, AppliesTransformTranslate) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "shift");

    std::string css = R"(.shift { transform: translate(12px, 4px); })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_TRUE(style->transform_has_translate);
    EXPECT_FLOAT_EQ(style->transform_translate_x, 12.0f);
    EXPECT_FLOAT_EQ(style->transform_translate_y, 4.0f);
}

TEST(StyleEngineTest, AppliesOpacityAndClampsRange) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "faded");

    std::string css = R"(.faded { opacity: 150%; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->opacity, 1.0f);

    std::string css_low = R"(.faded { opacity: 25%; })";
    Parser parser_low(css_low);
    auto sheet_low = parser_low.parse();
    engine.apply(sheet_low, root.get());
    style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->opacity, 0.25f);
}

TEST(StyleEngineTest, CascadesBySpecificityAndOrder) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    root->set_attribute(Attr::Class, "text");
    root->set_attribute(Attr::Id, "main");

    std::string css = R"(
        p { color: blue; margin: 1px; }
        .text { color: red; margin: 2px; }
        #main { color: black; margin: 3px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.r, 0);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 0);
    EXPECT_FLOAT_EQ(style->margin.top, 3.0f);
}

TEST(StyleEngineTest, AuthorColorOverridesAnchorDefaults) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);
    root->append_child(DomFactory::create_text(arena, "Link"));

    std::string css = "a { color: red; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.r, 255);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 0);
    EXPECT_TRUE(style->underline);
}

TEST(StyleEngineTest, SupportsTextDecorationUnderlineAndNone) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    auto link = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);
    link->append_child(DomFactory::create_text(arena, "Link"));
    root->append_child(std::move(link));

    auto span = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    span->set_attribute(Attr::Class, "uline");
    span->append_child(DomFactory::create_text(arena, "Underlined"));
    root->append_child(std::move(span));

    std::string css = "a { text-decoration: none; } .uline { text-decoration: underline; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto link_style = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(link_style);
    EXPECT_FALSE(link_style->underline);

    auto span_style = root->get_children()[1]->get_computed_style();
    ASSERT_TRUE(span_style);
    EXPECT_TRUE(span_style->underline);
}

TEST(StyleEngineTest, AppliesUnderlineThicknessAndOffset) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    root->set_attribute(Attr::Class, "u");

    std::string css =
        "p.u { text-decoration: underline; text-decoration-thickness: 3px; "
        "text-underline-offset: 4px; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_TRUE(style->underline);
    ASSERT_TRUE(style->underline_thickness.has_value());
    EXPECT_FLOAT_EQ(style->underline_thickness.value(), 3.0f);
    ASSERT_TRUE(style->underline_offset.has_value());
    EXPECT_FLOAT_EQ(style->underline_offset.value(), 4.0f);
}

TEST(StyleEngineTest, LaterRuleWinsOnEqualSpecificity) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "box");

    std::string css = R"(
        .box { margin: 4px; }
        .box { margin: 9px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->margin.top, 9.0f);
}

TEST(StyleEngineTest, ExpandsBackgroundAndBorderShorthand) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { background: #ff0000; border: 2px solid #000; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->background.has_value());
    EXPECT_EQ(style->background->r, 255);
    EXPECT_EQ(style->background->g, 0);
    EXPECT_EQ(style->background->b, 0);

    EXPECT_EQ(style->border_style, ComputedStyle::BorderStyle::Solid);
    EXPECT_FLOAT_EQ(style->border_width.top, 2.0f);
    EXPECT_FLOAT_EQ(style->border_width.right, 2.0f);
    EXPECT_FLOAT_EQ(style->border_width.bottom, 2.0f);
    EXPECT_FLOAT_EQ(style->border_width.left, 2.0f);
    EXPECT_EQ(style->border_color.r, 0);
    EXPECT_EQ(style->border_color.g, 0);
    EXPECT_EQ(style->border_color.b, 0);
}

TEST(StyleEngineTest, SupportsMarginAutoAndMaxWidth) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { margin: 8px auto; max-width: 200px; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->margin.top, 8.0f);
    EXPECT_FLOAT_EQ(style->margin.bottom, 8.0f);
    EXPECT_TRUE(style->margin_left_auto);
    EXPECT_TRUE(style->margin_right_auto);
    ASSERT_TRUE(style->max_width.has_value());
    EXPECT_FLOAT_EQ(style->max_width.value(), 200.0f);
}

TEST(StyleEngineTest, AppliesMinMaxSizeProperties) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "box");

    std::string css = R"(.box { min-width: 80px; min-height: 20px; max-width: 160px; max-height: 40px; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->min_width.has_value());
    EXPECT_FLOAT_EQ(style->min_width.value(), 80.0f);
    ASSERT_TRUE(style->min_height.has_value());
    EXPECT_FLOAT_EQ(style->min_height.value(), 20.0f);
    ASSERT_TRUE(style->max_width.has_value());
    EXPECT_FLOAT_EQ(style->max_width.value(), 160.0f);
    ASSERT_TRUE(style->max_height.has_value());
    EXPECT_FLOAT_EQ(style->max_height.value(), 40.0f);
}

TEST(StyleEngineTest, InheritsListStyleFromParent) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto ul = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Ul);
    auto li = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Li);
    ul->append_child(std::move(li));

    std::string css = R"(ul { list-style: none; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, ul.get());

    auto ul_style = ul->get_computed_style();
    auto li_style = ul->get_children()[0]->get_computed_style();
    ASSERT_TRUE(ul_style);
    ASSERT_TRUE(li_style);
    EXPECT_EQ(ul_style->list_style_type, ComputedStyle::ListStyleType::None);
    EXPECT_EQ(li_style->list_style_type, ComputedStyle::ListStyleType::None);
}

TEST(StyleEngineTest, IgnoresMaxWidthWithUnknownUnit) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { max-width: 40ch; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FALSE(style->max_width.has_value());
}

TEST(StyleEngineTest, SupportsEmUnitsForFontSizeAndSpacing) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto span = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    span->append_child(DomFactory::create_text(arena, "Hello"));
    root->append_child(std::move(span));

    std::string css = "div { font-size: 20px; } span { font-size: 1.5em; margin-top: 2em; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto root_style = root->get_computed_style();
    ASSERT_TRUE(root_style);
    EXPECT_FLOAT_EQ(root_style->font_size, 20.0f);

    auto span_style = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(span_style);
    EXPECT_FLOAT_EQ(span_style->font_size, 30.0f);
    EXPECT_FLOAT_EQ(span_style->margin.top, 60.0f);
}

TEST(StyleEngineTest, AppliesTextAlignFromCss) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string css = "p { text-align: center; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->text_align, ComputedStyle::TextAlign::Center);
}

TEST(StyleEngineTest, AppliesWhiteSpaceNoWrapFromCss) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string css = "p { white-space: nowrap; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->whitespace, ComputedStyle::WhiteSpace::NoWrap);
}

TEST(StyleEngineTest, AppliesFontFamilyFromCss) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string css = "p { font-family: Roboto Mono, sans-serif; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->font_face, "roboto mono,sans-serif");
}

TEST(StyleEngineTest, AppliesFontWeightAndStyleFromCss) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string css = "p { font-weight: bold; font-style: italic; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->weight, ComputedStyle::FontWeight::Bold);
    EXPECT_EQ(style->style, ComputedStyle::FontStyle::Italic);
}

TEST(StyleEngineTest, AppliesNumericFontWeight) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string css = "p { font-weight: 700; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->weight, ComputedStyle::FontWeight::Bold);
}

TEST(StyleEngineTest, AppliesFontSizeAndLineHeight) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string css = "p { font-size: 20px; line-height: 1.5; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->font_size, 20.0f);
    EXPECT_FLOAT_EQ(style->line_height, 30.0f);
}

TEST(StyleEngineTest, LinkSourcesApplyInDocumentOrder) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string ua = "p { color: red; }";
    std::vector<std::string> links = {"p { color: blue; }", "p { color: black; }"};
    std::vector<std::string> blocks;

    auto merged = Hummingbird::Css::merge_css_sources(ua, links, blocks);
    Parser parser(merged);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.r, 0);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 0);
}

TEST(StyleEngineTest, StyleBlocksOverrideLinksInOrder) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string ua = "p { color: red; }";
    std::vector<std::string> links = {"p { color: blue; }", "p { color: black; }"};
    std::vector<std::string> blocks = {"p { color: white; }", "p { color: blue; }"};

    auto merged = Hummingbird::Css::merge_css_sources(ua, links, blocks);
    Parser parser(merged);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.r, 0);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 255);
}

TEST(StyleEngineTest, ExtensionSourcesOverrideAuthorBlocks) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string ua = "p { color: red; }";
    std::vector<std::string> links = {"p { color: blue; }"};
    std::vector<std::string> blocks = {"p { color: green; }"};
    std::vector<std::string> extensions = {"p { color: black; }"};

    auto merged = Hummingbird::Css::merge_css_sources(ua, links, blocks, extensions);
    Parser parser(merged);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.r, 0);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 0);
}

TEST(StyleEngineTest, AppliesBorderProperties) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = R"(
        div { border-width: 2px; border-style: solid; border-color: #cc0000; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->border_style, ComputedStyle::BorderStyle::Solid);
    EXPECT_FLOAT_EQ(style->border_width.top, 2.0f);
    EXPECT_EQ(style->border_color.r, 0xcc);
    EXPECT_EQ(style->border_color.g, 0x00);
    EXPECT_EQ(style->border_color.b, 0x00);
}

TEST(StyleEngineTest, AppliesBorderRadiusProperty) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Id, "rounded");

    std::string css = R"(#rounded { border-radius: 12px; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->border_radius, 12.0f);
}

TEST(StyleEngineTest, AppliesOutlineAndOffsetProperties) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Id, "outlined");

    std::string css = R"(#outlined { outline: 3px solid #336699; outline-offset: 2px; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->outline_width, 3.0f);
    EXPECT_FLOAT_EQ(style->outline_offset, 2.0f);
    EXPECT_EQ(style->outline_color.r, 0x33);
    EXPECT_EQ(style->outline_color.g, 0x66);
    EXPECT_EQ(style->outline_color.b, 0x99);
}

TEST(StyleEngineTest, AppliesBackgroundColor) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { background-color: #333; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->background.has_value());
    EXPECT_EQ(style->background->r, 51);
    EXPECT_EQ(style->background->g, 51);
    EXPECT_EQ(style->background->b, 51);
}

TEST(StyleEngineTest, AppliesPositionProperties) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { position: absolute; top: 4px; left: 6px; z-index: 3; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->position, ComputedStyle::Position::Absolute);
    ASSERT_TRUE(style->top.has_value());
    ASSERT_TRUE(style->left.has_value());
    EXPECT_FLOAT_EQ(*style->top, 4.0f);
    EXPECT_FLOAT_EQ(*style->left, 6.0f);
    ASSERT_TRUE(style->z_index.has_value());
    EXPECT_EQ(*style->z_index, 3);
}

TEST(StyleEngineTest, AppliesBackgroundImageProperties) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css =
        "div { background-image: url(/img/logo.png); background-repeat: no-repeat; background-position: center; "
        "background-size: contain; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->background_image.has_value());
    EXPECT_EQ(*style->background_image, "/img/logo.png");
    EXPECT_EQ(style->background_repeat, ComputedStyle::BackgroundRepeat::NoRepeat);
    EXPECT_EQ(style->background_position.horizontal, ComputedStyle::BackgroundPosition::Horizontal::Center);
    EXPECT_EQ(style->background_position.vertical, ComputedStyle::BackgroundPosition::Vertical::Center);
    EXPECT_EQ(style->background_size.type, ComputedStyle::BackgroundSize::Type::Contain);
}

TEST(StyleEngineTest, AppliesInlineBlockDisplay) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { display: inline-block; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->display, ComputedStyle::Display::InlineBlock);
}

TEST(StyleEngineTest, DefaultsListItemDisplay) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Li);

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->display, ComputedStyle::Display::ListItem);
}

TEST(StyleEngineTest, EmInheritsHeadingTypography) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto h1 = DomFactory::create_element(arena, Hummingbird::Html::TagNames::H1);
    auto em = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Em);
    em->append_child(DomFactory::create_text(arena, "Emphasized"));
    h1->append_child(std::move(em));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, h1.get());

    auto h1_style = h1->get_computed_style();
    ASSERT_TRUE(h1_style);
    auto em_style = h1->get_children()[0]->get_computed_style();
    ASSERT_TRUE(em_style);

    EXPECT_GT(h1_style->font_size, 16.0f);
    EXPECT_EQ(h1_style->weight, ComputedStyle::FontWeight::Bold);
    EXPECT_FLOAT_EQ(em_style->font_size, h1_style->font_size);
    EXPECT_EQ(em_style->weight, h1_style->weight);
    EXPECT_EQ(em_style->style, ComputedStyle::FontStyle::Italic);
}

TEST(StyleEngineTest, AlignAttributeMapsToTextAlign) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto cell = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Td);
    cell->set_attribute(Attr::Align, "center");
    auto span = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    span->append_child(DomFactory::create_text(arena, "Text"));
    cell->append_child(std::move(span));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, cell.get());

    auto cell_style = cell->get_computed_style();
    ASSERT_TRUE(cell_style);
    EXPECT_EQ(cell_style->text_align, ComputedStyle::TextAlign::Center);

    auto child_style = cell->get_children()[0]->get_computed_style();
    ASSERT_TRUE(child_style);
    EXPECT_EQ(child_style->text_align, ComputedStyle::TextAlign::Center);
}

TEST(StyleEngineTest, NoWrapAttributeMapsToWhiteSpace) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto cell = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Td);
    cell->set_attribute(Attr::NoWrap, "");
    cell->append_child(DomFactory::create_text(arena, "Text"));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, cell.get());

    auto cell_style = cell->get_computed_style();
    ASSERT_TRUE(cell_style);
    EXPECT_EQ(cell_style->whitespace, ComputedStyle::WhiteSpace::NoWrap);

    auto child_style = cell->get_children()[0]->get_computed_style();
    ASSERT_TRUE(child_style);
    EXPECT_EQ(child_style->whitespace, ComputedStyle::WhiteSpace::NoWrap);
}

TEST(StyleEngineTest, BodyAttributesMapToColors) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto body = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Body);
    body->set_attribute(Attr::BgColor, "#99cc99");
    body->set_attribute(Attr::Text, "#112233");
    body->set_attribute(Attr::Link, "#445566");
    body->set_attribute(Attr::VLink, "#778899");

    auto link = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);
    link->append_child(DomFactory::create_text(arena, "Link"));
    body->append_child(std::move(link));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, body.get());

    auto body_style = body->get_computed_style();
    ASSERT_TRUE(body_style);
    ASSERT_TRUE(body_style->background.has_value());
    EXPECT_EQ(body_style->background->r, 0x99);
    EXPECT_EQ(body_style->background->g, 0xcc);
    EXPECT_EQ(body_style->background->b, 0x99);
    EXPECT_EQ(body_style->color.r, 0x11);
    EXPECT_EQ(body_style->color.g, 0x22);
    EXPECT_EQ(body_style->color.b, 0x33);
    ASSERT_TRUE(body_style->link_color.has_value());
    EXPECT_EQ(body_style->link_color->r, 0x44);
    EXPECT_EQ(body_style->link_color->g, 0x55);
    EXPECT_EQ(body_style->link_color->b, 0x66);
    ASSERT_TRUE(body_style->vlink_color.has_value());
    EXPECT_EQ(body_style->vlink_color->r, 0x77);
    EXPECT_EQ(body_style->vlink_color->g, 0x88);
    EXPECT_EQ(body_style->vlink_color->b, 0x99);

    auto link_style = body->get_children()[0]->get_computed_style();
    ASSERT_TRUE(link_style);
    EXPECT_EQ(link_style->color.r, 0x44);
    EXPECT_EQ(link_style->color.g, 0x55);
    EXPECT_EQ(link_style->color.b, 0x66);
    EXPECT_TRUE(link_style->underline);
}

TEST(StyleEngineTest, WidthHeightAttributesMapToStyleWhenUnset) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto img = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Img);
    img->set_attribute(Attr::Width, "120");
    img->set_attribute(Attr::Height, "80");

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, img.get());

    auto img_style = img->get_computed_style();
    ASSERT_TRUE(img_style);
    ASSERT_TRUE(img_style->width.has_value());
    ASSERT_TRUE(img_style->height.has_value());
    EXPECT_FLOAT_EQ(img_style->width.value(), 120.0f);
    EXPECT_FLOAT_EQ(img_style->height.value(), 80.0f);
}

TEST(StyleEngineTest, FontTagMapsSizeAndFace) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto font = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Font);
    font->set_attribute(Attr::Size, "6");
    font->set_attribute(Attr::Face, "Sans-Serif");
    font->append_child(DomFactory::create_text(arena, "Hello"));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, font.get());

    auto font_style = font->get_computed_style();
    ASSERT_TRUE(font_style);
    EXPECT_FLOAT_EQ(font_style->font_size, 32.0f);
    EXPECT_EQ(font_style->font_face, "sans-serif");
    EXPECT_EQ(font_style->display, ComputedStyle::Display::Inline);
}

TEST(StyleEngineTest, AppliesTextEffectsProperties) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "text-fx");

    std::string css =
        R"(.text-fx { text-transform: uppercase; letter-spacing: 2px; text-indent: 12px; text-overflow: ellipsis; word-wrap: break-word; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->text_transform, ComputedStyle::TextTransform::Uppercase);
    EXPECT_FLOAT_EQ(style->letter_spacing, 2.0f);
    EXPECT_FLOAT_EQ(style->text_indent, 12.0f);
    EXPECT_EQ(style->text_overflow, ComputedStyle::TextOverflow::Ellipsis);
    EXPECT_EQ(style->word_wrap, ComputedStyle::WordWrap::BreakWord);
}
