#include "style/StyleEngine.h"

#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "html/HtmlTagNames.h"
#include "style/CssParser.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;

TEST(StyleEngineTest, AppliesRulesAndCascade) {
    // DOM: <div class="box" id="main"><span></span></div>
    ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute("class", "box");
    root->set_attribute("id", "main");
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
    ArenaAllocator arena(2048);
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
    ASSERT_TRUE(code_style->background.has_value());
    EXPECT_GT(code_style->padding.left, 0.0f);

    EXPECT_FLOAT_EQ(blockquote_style->margin.left, 40.0f);
    EXPECT_TRUE(hr_style->height.has_value());
    EXPECT_GT(hr_style->height.value(), 0.0f);

    EXPECT_GT(h1_style->font_size, 16.0f);
    EXPECT_EQ(h1_style->weight, ComputedStyle::FontWeight::Bold);
}

TEST(StyleEngineTest, CascadesBySpecificityAndOrder) {
    ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    root->set_attribute("class", "text");
    root->set_attribute("id", "main");

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
    ArenaAllocator arena(2048);
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

TEST(StyleEngineTest, LaterRuleWinsOnEqualSpecificity) {
    ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute("class", "box");

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

TEST(StyleEngineTest, AppliesBorderProperties) {
    ArenaAllocator arena(2048);
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

TEST(StyleEngineTest, AppliesBackgroundColor) {
    ArenaAllocator arena(2048);
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

TEST(StyleEngineTest, AppliesInlineBlockDisplay) {
    ArenaAllocator arena(2048);
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
    ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Li);

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->display, ComputedStyle::Display::ListItem);
}

TEST(StyleEngineTest, EmInheritsHeadingTypography) {
    ArenaAllocator arena(2048);
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
    ArenaAllocator arena(2048);
    auto cell = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Td);
    cell->set_attribute("align", "center");
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
    ArenaAllocator arena(2048);
    auto cell = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Td);
    cell->set_attribute("nowrap", "");
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

TEST(StyleEngineTest, WidthHeightAttributesMapToStyleWhenUnset) {
    ArenaAllocator arena(2048);
    auto img = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Img);
    img->set_attribute("width", "120");
    img->set_attribute("height", "80");

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
