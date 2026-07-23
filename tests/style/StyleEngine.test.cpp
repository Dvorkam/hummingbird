#include "style/compute/StyleEngine.h"

#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "style/compute/FontFaceRegistry.h"
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
    EXPECT_FLOAT_EQ(style_root->width->px, 80);

    // Child span should at least have a computed style object (even if empty).
    auto style_child = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(style_child);
}

TEST(StyleEngineTest, FontSrcResolvedFromFontFaceRegistry) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "icon");

    Parser parser(".icon { font-family: \"My Icons\", sans-serif; }");
    auto sheet = parser.parse();

    FontFaceRegistry fonts;
    fonts.register_family("my icons", "fonts/icons.ttf");

    StyleEngine engine;
    engine.apply(sheet, root.get(), {}, &fonts);

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->font_src, "fonts/icons.ttf");
}

TEST(StyleEngineTest, FontSrcEmptyWhenNoFontFaceMatches) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    Parser parser("div { font-family: sans-serif; }");
    auto sheet = parser.parse();

    FontFaceRegistry fonts;
    fonts.register_family("my icons", "fonts/icons.ttf");

    StyleEngine engine;
    engine.apply(sheet, root.get(), {}, &fonts);

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_TRUE(style->font_src.empty());
}

TEST(StyleEngineTest, FontSrcInheritsThroughFontFamilyInheritance) {
    // A child with no font-family of its own inherits the parent's, so the web
    // font must resolve on the child too.
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "icon");
    root->append_child(DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span));

    Parser parser(".icon { font-family: myicons; }");
    auto sheet = parser.parse();

    FontFaceRegistry fonts;
    fonts.register_family("myicons", "fonts/icons.ttf");

    StyleEngine engine;
    engine.apply(sheet, root.get(), {}, &fonts);

    auto child = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(child);
    EXPECT_EQ(child->font_src, "fonts/icons.ttf");
}

TEST(StyleEngineTest, GridTemplateAndPlacementComputed) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "grid");
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    child->set_attribute(Attr::Class, "cell");
    root->append_child(std::move(child));

    // repeat() must be the last track-list component in this MVP (the tokenizer
    // drops the parens, so a trailing track can't be told apart from the repeat).
    Parser parser(
        ".grid { display: grid; grid-template-columns: 100px repeat(2, 1fr); gap: 12px 8px; } "
        ".cell { grid-column: 1 / 3; }");
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->display, ComputedStyle::Display::Grid);
    // 100px repeat(2, 1fr) -> 100px, 1fr, 1fr.
    ASSERT_EQ(style->grid_template_columns.size(), 3u);
    EXPECT_EQ(style->grid_template_columns[0].kind, ComputedStyle::GridTrack::Kind::Fixed);
    EXPECT_FLOAT_EQ(style->grid_template_columns[0].value, 100.0f);
    EXPECT_EQ(style->grid_template_columns[2].kind, ComputedStyle::GridTrack::Kind::Fr);
    EXPECT_FLOAT_EQ(style->grid_template_columns[2].value, 1.0f);
    // gap: 12px 8px -> row 12, column 8.
    EXPECT_FLOAT_EQ(style->row_gap, 12.0f);
    EXPECT_FLOAT_EQ(style->column_gap, 8.0f);

    auto cell = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(cell);
    EXPECT_EQ(cell->grid_column.line, 1);
    EXPECT_EQ(cell->grid_column.span, 2);  // 1 / 3 -> span 2
}

TEST(StyleEngineTest, VisitedAnchorUsesVlinkColor) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto body = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Body);
    body->set_attribute(Attr::Link, "#112233");
    body->set_attribute(Attr::VLink, "#778899");

    auto visited = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);
    visited->set_pseudo_state(Element::PseudoState::Visited, true);
    auto fresh = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);
    body->append_child(std::move(visited));
    body->append_child(std::move(fresh));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, body.get());

    auto visited_style = body->get_children()[0]->get_computed_style();
    auto fresh_style = body->get_children()[1]->get_computed_style();
    ASSERT_TRUE(visited_style && fresh_style);
    // Visited -> vlink (#778899); unvisited -> link (#112233).
    EXPECT_EQ(visited_style->color.r, 0x77);
    EXPECT_EQ(visited_style->color.b, 0x99);
    EXPECT_EQ(fresh_style->color.r, 0x11);
    EXPECT_EQ(fresh_style->color.b, 0x33);
}

TEST(StyleEngineTest, VisitedPseudoClassMatchesOnlyVisitedAnchors) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto visited = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);
    visited->set_pseudo_state(Element::PseudoState::Visited, true);
    auto fresh = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);
    root->append_child(std::move(visited));
    root->append_child(std::move(fresh));

    std::string css = "a:visited { color: #00ff00; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto visited_style = root->get_children()[0]->get_computed_style();
    auto fresh_style = root->get_children()[1]->get_computed_style();
    ASSERT_TRUE(visited_style && fresh_style);
    EXPECT_EQ(visited_style->color.g, 0xff);  // :visited rule applied
    // The unvisited anchor keeps the UA default link blue, not green.
    EXPECT_NE(fresh_style->color.g, 0xff);
    EXPECT_EQ(fresh_style->color.b, 0xff);
}

TEST(StyleEngineTest, CascadeOrderPreservedAcrossSelectorBuckets) {
    // The key-selector index buckets these three rules separately (class, tag,
    // class); the winner must still be decided by specificity then document
    // order after they are re-merged (T-PERF-STYLE-1 must not reorder cascade).
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "a");

    // .a (spec 10) beats div (spec 1); between the two equal-specificity .a
    // rules, the later one wins -> green.
    std::string css = R"(.a { color: #ff0000; } div { color: #0000ff; } .a { color: #00ff00; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.g, 0xff);
    EXPECT_EQ(style->color.r, 0x00);
    EXPECT_EQ(style->color.b, 0x00);
}

TEST(StyleEngineTest, UniversalAndIdBucketsBothConsidered) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    root->set_attribute(Attr::Id, "x");

    // Universal rule applies, but the id rule (spec 100) wins the color.
    std::string css = R"(* { color: #ff0000; letter-spacing: 2px; } #x { color: #0000ff; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.b, 0xff);               // from #x
    EXPECT_FLOAT_EQ(style->letter_spacing, 2.0f);  // from *
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
    EXPECT_FALSE(pre_style->background.has_value());

    EXPECT_EQ(a_style->color.r, 0);
    EXPECT_EQ(a_style->color.g, 0);
    EXPECT_EQ(a_style->color.b, 255);
    EXPECT_TRUE(a_style->underline);

    EXPECT_TRUE(code_style->font_monospace);
    EXPECT_FALSE(code_style->background.has_value());
    EXPECT_GT(code_style->padding.left, 0.0f);

    EXPECT_FLOAT_EQ(blockquote_style->margin.left, 40.0f);
    EXPECT_TRUE(hr_style->height.has_value());
    EXPECT_GT(hr_style->height->px, 0.0f);

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
    EXPECT_FLOAT_EQ(submit_style->width->px, 80.0f);
    EXPECT_TRUE(text_style->width.has_value());
    EXPECT_FLOAT_EQ(text_style->width->px, 180.0f);
}

TEST(StyleEngineTest, HiddenInputIsNotRendered) {
    // HN's comment form carries `parent`, `goto`, and `hmac` as
    // `input[type=hidden]`. They must be display:none, not painted as stray
    // button boxes next to the textarea.
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto hidden = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    hidden->set_attribute(Attr::Type, "hidden");

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, hidden.get());

    auto style = hidden->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->display, ComputedStyle::Display::None);
}

TEST(StyleEngineTest, TableCellsUseDefaultPaddingForReadability) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto td = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Td);
    auto th = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Th);

    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Table);
    root->append_child(std::move(td));
    root->append_child(std::move(th));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, root.get());

    auto td_style = dynamic_cast<Element*>(root->get_children()[0].get())->get_computed_style();
    auto th_style = dynamic_cast<Element*>(root->get_children()[1].get())->get_computed_style();
    ASSERT_TRUE(td_style);
    ASSERT_TRUE(th_style);

    EXPECT_FLOAT_EQ(td_style->padding.left, 2.0f);
    EXPECT_FLOAT_EQ(td_style->padding.right, 2.0f);
    EXPECT_FLOAT_EQ(td_style->padding.top, 2.0f);
    EXPECT_FLOAT_EQ(td_style->padding.bottom, 2.0f);

    EXPECT_FLOAT_EQ(th_style->padding.left, 2.0f);
    EXPECT_FLOAT_EQ(th_style->padding.right, 2.0f);
    EXPECT_FLOAT_EQ(th_style->padding.top, 2.0f);
    EXPECT_FLOAT_EQ(th_style->padding.bottom, 2.0f);
}

TEST(StyleEngineTest, VisibilityAndPointerEventsInheritAndReEnable) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    auto grandchild = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    grandchild->set_attribute(Attr::Class, "show");

    child->append_child(std::move(grandchild));
    root->append_child(std::move(child));

    std::string css = R"(div { visibility: hidden; pointer-events: none; }
                         .show { visibility: visible; pointer-events: auto; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto root_style = root->get_computed_style();
    auto child_style = root->get_children()[0]->get_computed_style();
    auto grandchild_style = root->get_children()[0]->get_children()[0]->get_computed_style();
    ASSERT_TRUE(root_style && child_style && grandchild_style);

    EXPECT_EQ(root_style->visibility, ComputedStyle::Visibility::Hidden);
    EXPECT_EQ(root_style->pointer_events, ComputedStyle::PointerEvents::None);
    // The <p> sets neither property, so it inherits hidden / none from <div>.
    EXPECT_EQ(child_style->visibility, ComputedStyle::Visibility::Hidden);
    EXPECT_EQ(child_style->pointer_events, ComputedStyle::PointerEvents::None);
    // The .show <span> re-enables both.
    EXPECT_EQ(grandchild_style->visibility, ComputedStyle::Visibility::Visible);
    EXPECT_EQ(grandchild_style->pointer_events, ComputedStyle::PointerEvents::Auto);
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

TEST(StyleEngineTest, AppliesClearProperty) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "cleared");

    std::string css = R"(.cleared { clear: both; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->clear, ComputedStyle::Clear::Both);
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
    EXPECT_FLOAT_EQ(style->width->percent, 70.0f);
    EXPECT_TRUE(style->width->has_percent);
    ASSERT_TRUE(style->top.has_value());
    EXPECT_FLOAT_EQ(style->top->percent, 24.0f);
    EXPECT_TRUE(style->top->has_percent);
    ASSERT_TRUE(style->left.has_value());
    EXPECT_FLOAT_EQ(style->left->percent, 10.0f);
    EXPECT_TRUE(style->left->has_percent);
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

TEST(StyleEngineTest, ImportantBeatsSpecificityAndOrder) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    root->set_attribute(Attr::Class, "text");
    root->set_attribute(Attr::Id, "main");

    // The dark-mode-extension shape: a low-specificity important rule must beat
    // higher-specificity and later normal rules (T-CSS-IMPORTANT-1).
    std::string css = R"(
        p { color: blue !important; margin: 1px; }
        .text { color: red; margin: 2px !important; }
        #main { color: black; margin: 3px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    // p!important beats .text and #main.
    EXPECT_EQ(style->color.r, 0);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 255);
    // .text!important beats #main.
    EXPECT_FLOAT_EQ(style->margin.top, 2.0f);
}

TEST(StyleEngineTest, LaterImportantWinsAmongImportant) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string css = R"(
        p { color: blue !important; }
        p { color: red !important; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.r, 255);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 0);
}

TEST(StyleEngineTest, InheritKeywordCopiesParentValue) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    root->append_child(std::move(child));

    // DDG: `.search__input { font-family: inherit }` must take the parent's
    // font, not warn and fall back (T-CSS-INHERIT-1).
    std::string css = R"(
        div { font-family: Arial; color: red; }
        input { font-family: inherit; color: inherit; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->font_face, "arial");
    EXPECT_EQ(style->color.r, 255);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 0);
}

TEST(StyleEngineTest, InheritWinsCascadeOverConcreteValue) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    child->set_attribute(Attr::Class, "quiet");
    root->append_child(std::move(child));

    // `.quiet { color: inherit }` outranks `p { color: red }`; the winning
    // declaration is `inherit`, so the parent's blue applies — not red.
    std::string css = R"(
        div { color: blue; }
        p { color: red; }
        .quiet { color: inherit; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->color.r, 0);
    EXPECT_EQ(style->color.g, 0);
    EXPECT_EQ(style->color.b, 255);
}

TEST(StyleEngineTest, ResolvesVarForLengthProperties) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);
    root->append_child(std::move(child));

    // DDG: --default-border-radius / --max-content-width feed length props.
    std::string css = R"(
        div { --default-border-radius: 4px; --max-content-width: 590px; }
        p { border-radius: var(--default-border-radius); max-width: var(--max-content-width); }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->border_radius.top_left.value, 4.0f);
    EXPECT_FLOAT_EQ(style->border_radius.bottom_right.value, 4.0f);
    ASSERT_TRUE(style->max_width.has_value());
    EXPECT_FLOAT_EQ(style->max_width->px, 590.0f);
}

TEST(StyleEngineTest, ResolvesVarInsideBorderRadiusList) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    // DDG button: `border-radius: 0 var(--r) var(--r) 0` — var terms mixed
    // with plain lengths inside one shorthand.
    std::string css = R"(
        div { --r: 4px; border-radius: 0 var(--r) var(--r) 0; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->border_radius.top_left.value, 0.0f);
    EXPECT_FLOAT_EQ(style->border_radius.top_right.value, 4.0f);
    EXPECT_FLOAT_EQ(style->border_radius.bottom_right.value, 4.0f);
    EXPECT_FLOAT_EQ(style->border_radius.bottom_left.value, 0.0f);
}

TEST(StyleEngineTest, VarFallbackAppliesWhenUnset) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { max-width: var(--missing, 320px); }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->max_width.has_value());
    EXPECT_FLOAT_EQ(style->max_width->px, 320.0f);
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
    EXPECT_FLOAT_EQ(style->max_width->px, 200.0f);
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
    EXPECT_FLOAT_EQ(style->min_width->px, 80.0f);
    ASSERT_TRUE(style->min_height.has_value());
    EXPECT_FLOAT_EQ(style->min_height->px, 20.0f);
    ASSERT_TRUE(style->max_width.has_value());
    EXPECT_FLOAT_EQ(style->max_width->px, 160.0f);
    ASSERT_TRUE(style->max_height.has_value());
    EXPECT_FLOAT_EQ(style->max_height->px, 40.0f);
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

TEST(StyleEngineTest, AppliesDecimalListStyleType) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto ol = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Ol);
    auto li = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Li);
    ol->append_child(std::move(li));

    std::string css = R"(ol { list-style-type: decimal; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, ol.get());

    auto ol_style = ol->get_computed_style();
    auto li_style = ol->get_children()[0]->get_computed_style();
    ASSERT_TRUE(ol_style);
    ASSERT_TRUE(li_style);
    EXPECT_EQ(ol_style->list_style_type, ComputedStyle::ListStyleType::Decimal);
    EXPECT_EQ(li_style->list_style_type, ComputedStyle::ListStyleType::Decimal);
}

TEST(StyleEngineTest, AppliesOrderedListUaDefault) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto ol = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Ol);
    auto li = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Li);
    ol->append_child(std::move(li));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, ol.get());

    auto ol_style = ol->get_computed_style();
    auto li_style = ol->get_children()[0]->get_computed_style();
    ASSERT_TRUE(ol_style);
    ASSERT_TRUE(li_style);
    EXPECT_EQ(ol_style->list_style_type, ComputedStyle::ListStyleType::Decimal);
    EXPECT_EQ(li_style->list_style_type, ComputedStyle::ListStyleType::Decimal);
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

TEST(StyleEngineTest, AppliesFontShorthandComponents) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::P);

    std::string css = "p { font: italic 700 20px/1.5 Roboto Mono, monospace; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->style, ComputedStyle::FontStyle::Italic);
    EXPECT_EQ(style->weight, ComputedStyle::FontWeight::Bold);
    EXPECT_FLOAT_EQ(style->font_size, 20.0f);
    EXPECT_FLOAT_EQ(style->line_height, 30.0f);
    EXPECT_EQ(style->font_face, "Roboto Mono monospace");
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

TEST(StyleEngineTest, AppliesBorderSideShorthandsToIndividualWidths) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = R"(
        div {
            border-top: 5px solid #224488;
            border-right-width: 3px;
            border-bottom: 2px inset #224488;
        }
    )";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->border_width.top, 5.0f);
    EXPECT_FLOAT_EQ(style->border_width.right, 3.0f);
    EXPECT_FLOAT_EQ(style->border_width.bottom, 2.0f);
    EXPECT_FLOAT_EQ(style->border_width.left, 0.0f);
    EXPECT_EQ(style->border_style, ComputedStyle::BorderStyle::Inset);
    EXPECT_EQ(style->border_color.r, 0x22);
    EXPECT_EQ(style->border_color.g, 0x44);
    EXPECT_EQ(style->border_color.b, 0x88);
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
    EXPECT_TRUE(style->border_radius.uniform());
    EXPECT_FLOAT_EQ(style->border_radius.top_left.value, 12.0f);
    EXPECT_FLOAT_EQ(style->border_radius.bottom_right.value, 12.0f);
}

TEST(StyleEngineTest, AppliesPerCornerAndVendorBorderRadius) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Id, "corners");

    // Right-side-only rounding, DDG search-button style, plus a vendor alias
    // that must resolve to the standard property.
    std::string css = R"(#corners {
        border-radius: 0 4px 4px 0;
        -webkit-border-top-left-radius: 9px;
    })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->border_radius.top_left.value, 9.0f);  // vendor longhand wins by order
    EXPECT_FLOAT_EQ(style->border_radius.top_right.value, 4.0f);
    EXPECT_FLOAT_EQ(style->border_radius.bottom_right.value, 4.0f);
    EXPECT_FLOAT_EQ(style->border_radius.bottom_left.value, 0.0f);
    EXPECT_FALSE(style->border_radius.uniform());
}

TEST(StyleEngineTest, AppliesPercentBorderRadius) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Id, "circle");

    std::string css = R"(#circle { border-radius: 50%; })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_TRUE(style->border_radius.top_left.percent);
    EXPECT_FLOAT_EQ(style->border_radius.top_left.value, 50.0f);
    // 50% of a 40px box (min side) resolves to a 20px corner radius.
    EXPECT_FLOAT_EQ(style->border_radius.top_left.resolve(40.0f), 20.0f);
}

TEST(StyleEngineTest, AppliesPerSideBorderColor) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Id, "sides");

    std::string css = R"(#sides {
        border-color: #111111;
        border-left-color: #3969ef;
    })";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->border_edge_color.top.r, 0x11);
    EXPECT_EQ(style->border_edge_color.left.r, 0x39);
    EXPECT_EQ(style->border_edge_color.left.g, 0x69);
    EXPECT_EQ(style->border_edge_color.left.b, 0xef);
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
    EXPECT_FLOAT_EQ(style->top->px, 4.0f);
    EXPECT_FLOAT_EQ(style->left->px, 6.0f);
    ASSERT_TRUE(style->z_index.has_value());
    EXPECT_EQ(*style->z_index, 3);
}

TEST(StyleEngineTest, AppliesOverflowProperties) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { overflow: hidden; overflow-y: scroll; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->overflow_x, ComputedStyle::Overflow::Hidden);
    EXPECT_EQ(style->overflow_y, ComputedStyle::Overflow::Scroll);
}

TEST(StyleEngineTest, OverflowClipParsesAsHidden) {
    // `overflow: clip` maps to Hidden: the difference is scroll affordances,
    // which do not exist yet — both clip paint at the padding box (8.5.3).
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { overflow: clip; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->overflow_x, ComputedStyle::Overflow::Hidden);
    EXPECT_EQ(style->overflow_y, ComputedStyle::Overflow::Hidden);
}

TEST(StyleEngineTest, AppliesObjectFitProperty) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Img);

    std::string css = "img { object-fit: cover; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->object_fit, ComputedStyle::ObjectFit::Cover);
}

TEST(StyleEngineTest, AppliesCursorProperty) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::A);

    std::string css = "a { cursor: pointer; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->cursor, ComputedStyle::Cursor::Pointer);
}

TEST(StyleEngineTest, CursorIsInheritedToChildren) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "wrap");
    root->append_child(DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span));

    std::string css = ".wrap { cursor: text; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto parent_style = root->get_computed_style();
    ASSERT_TRUE(parent_style);
    EXPECT_EQ(parent_style->cursor, ComputedStyle::Cursor::Text);

    auto child_style = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(child_style);
    EXPECT_EQ(child_style->cursor, ComputedStyle::Cursor::Text);
}

// Drift guard for inherit_from_parent (T-STYLE-FIELDCOPY-1): a child that sets
// nothing must inherit every inherited text/font property from its parent. If a
// new inherited property is added but not wired into inherit_from_parent, one of
// these assertions fails instead of the bug surfacing silently at render time.
TEST(StyleEngineTest, InheritsAllInheritedProperties) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "p");
    root->append_child(DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span));

    std::string css = R"(.p {
        color: #ff0000;
        font-size: 20px;
        font-weight: bold;
        font-style: italic;
        text-align: center;
        text-transform: uppercase;
        letter-spacing: 3px;
        text-indent: 7px;
        white-space: nowrap;
        line-height: 30px;
        cursor: pointer;
        list-style-type: decimal;
        list-style-position: inside;
        word-wrap: break-word;
        text-decoration: underline;
    })";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto child = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(child);
    EXPECT_EQ(child->color.r, 255);
    EXPECT_EQ(child->color.g, 0);
    EXPECT_FLOAT_EQ(child->font_size, 20.0f);
    EXPECT_EQ(child->weight, ComputedStyle::FontWeight::Bold);
    EXPECT_EQ(child->style, ComputedStyle::FontStyle::Italic);
    EXPECT_EQ(child->text_align, ComputedStyle::TextAlign::Center);
    EXPECT_EQ(child->text_transform, ComputedStyle::TextTransform::Uppercase);
    EXPECT_FLOAT_EQ(child->letter_spacing, 3.0f);
    EXPECT_FLOAT_EQ(child->text_indent, 7.0f);
    EXPECT_EQ(child->whitespace, ComputedStyle::WhiteSpace::NoWrap);
    EXPECT_FLOAT_EQ(child->line_height, 30.0f);
    EXPECT_EQ(child->cursor, ComputedStyle::Cursor::Pointer);
    EXPECT_EQ(child->list_style_type, ComputedStyle::ListStyleType::Decimal);
    EXPECT_EQ(child->list_style_position, ComputedStyle::ListStylePosition::Inside);
    EXPECT_EQ(child->word_wrap, ComputedStyle::WordWrap::BreakWord);
    EXPECT_TRUE(child->underline);
}

// Core invariant of the inverted merge: non-inherited box properties must NOT
// leak from parent to child. This is what lets adding a new box property require
// no field-copy wiring at all.
TEST(StyleEngineTest, DoesNotInheritBoxPropertiesFromParent) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    root->set_attribute(Attr::Class, "p");
    root->append_child(DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span));

    std::string css = R"(.p {
        width: 100px;
        margin: 5px;
        padding: 4px;
        border: 2px solid #ff0000;
        border-radius: 6px;
        float: left;
        clear: both;
        position: relative;
        display: flex;
        z-index: 3;
    })";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto parent = root->get_computed_style();
    ASSERT_TRUE(parent);
    ASSERT_TRUE(parent->width.has_value());  // parent really did get the box props

    auto child = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(child);
    EXPECT_FALSE(child->width.has_value());
    EXPECT_FLOAT_EQ(child->margin.top, 0.0f);
    EXPECT_FLOAT_EQ(child->padding.top, 0.0f);
    EXPECT_EQ(child->border_style, ComputedStyle::BorderStyle::None);
    EXPECT_FLOAT_EQ(child->border_radius.top_left.value, 0.0f);
    EXPECT_EQ(child->float_type, ComputedStyle::Float::None);
    EXPECT_EQ(child->clear, ComputedStyle::Clear::None);
    EXPECT_EQ(child->position, ComputedStyle::Position::Static);
    EXPECT_NE(child->display, ComputedStyle::Display::Flex);
    EXPECT_FALSE(child->z_index.has_value());
}

TEST(StyleEngineTest, AppliesVerticalAlignProperty) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);

    std::string css = "span { vertical-align: middle; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->vertical_align, ComputedStyle::VerticalAlign::Middle);
}

TEST(StyleEngineTest, AppliesBoxShadowProperty) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { box-shadow: 3px 5px 7px #112233; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->box_shadow.has_value());
    EXPECT_FLOAT_EQ(style->box_shadow->offset_x, 3.0f);
    EXPECT_FLOAT_EQ(style->box_shadow->offset_y, 5.0f);
    EXPECT_FLOAT_EQ(style->box_shadow->blur, 7.0f);
    EXPECT_EQ(style->box_shadow->color.r, 0x11);
    EXPECT_EQ(style->box_shadow->color.g, 0x22);
    EXPECT_EQ(style->box_shadow->color.b, 0x33);
}

TEST(StyleEngineTest, ResolvesCalcWidthAgainstReference) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { width: calc(100% - 20px); }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->width.has_value());
    EXPECT_TRUE(style->width->has_percent);
    EXPECT_FLOAT_EQ(style->width->percent, 100.0f);
    EXPECT_FLOAT_EQ(style->width->px, -20.0f);
    // Resolved against a 200px containing block: 100% - 20px = 180px.
    EXPECT_FLOAT_EQ(style->width->resolve(200.0f), 180.0f);
}

TEST(StyleEngineTest, UnsupportedCalcLeavesLengthUnset) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { width: calc(100% - 2*6px); }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    // Multiplication is unsupported, so the width declaration is dropped.
    EXPECT_FALSE(style->width.has_value());
}

TEST(StyleEngineTest, LegacyClipRectHidesContentWhenEmpty) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto hidden = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    hidden->set_attribute(Attr::Class, "sr-only");
    auto boxed = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    boxed->set_attribute(Attr::Class, "framed");
    root->append_child(std::move(hidden));
    root->append_child(std::move(boxed));

    std::string css =
        ".sr-only { position: absolute; clip: rect(0 0 0 0); }"
        ".framed { position: absolute; clip: rect(0 20px 20px 0); }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto hidden_style = root->get_children()[0]->get_computed_style();
    auto boxed_style = root->get_children()[1]->get_computed_style();
    ASSERT_TRUE(hidden_style && hidden_style->clip.has_value());
    ASSERT_TRUE(boxed_style && boxed_style->clip.has_value());
    // rect(0 0 0 0) collapses to an empty region -> hides.
    EXPECT_TRUE(hidden_style->clip->hides_content());
    // rect(0 20px 20px 0) is a real 20x20 region -> does not hide.
    EXPECT_FALSE(boxed_style->clip->hides_content());
}

TEST(StyleEngineTest, AppliesAndInheritsTextShadow) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    root->append_child(std::move(child));

    std::string css = "div { text-shadow: 1px 2px 3px #112233; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->text_shadow.has_value());
    EXPECT_FLOAT_EQ(style->text_shadow->offset_x, 1.0f);
    EXPECT_FLOAT_EQ(style->text_shadow->offset_y, 2.0f);
    EXPECT_FLOAT_EQ(style->text_shadow->blur, 3.0f);
    EXPECT_EQ(style->text_shadow->color.r, 0x11);

    // text-shadow is inherited, so the child picks it up.
    auto child_style = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(child_style);
    ASSERT_TRUE(child_style->text_shadow.has_value());
    EXPECT_FLOAT_EQ(child_style->text_shadow->offset_y, 2.0f);
}

TEST(StyleEngineTest, TextShadowNoneClearsInheritedShadow) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    child->set_attribute(Attr::Class, "flat");
    root->append_child(std::move(child));

    std::string css = "div { text-shadow: 1px 1px 1px #000; } .flat { text-shadow: none; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto child_style = root->get_children()[0]->get_computed_style();
    ASSERT_TRUE(child_style);
    // `text-shadow: none` overrides the inherited shadow rather than keeping it.
    EXPECT_FALSE(child_style->text_shadow.has_value());
}

TEST(StyleEngineTest, AppliesPseudoClassFocusForInput) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    root->set_pseudo_state(Hummingbird::DOM::Element::PseudoState::Focus, true);

    std::string css = "input:focus { border-color: #ff0000; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->border_color.r, 255);
    EXPECT_EQ(style->border_color.g, 0);
    EXPECT_EQ(style->border_color.b, 0);
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

TEST(StyleEngineTest, ParsesPercentBackgroundSize) {
    // DDG logo: `background-size: 100%` must keep the percentage (resolved at
    // paint time) rather than degrading to 100px, which squished the SVG.
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { background-size: 100%; }";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->background_size.type, ComputedStyle::BackgroundSize::Type::Length);
    ASSERT_TRUE(style->background_size.width.has_value());
    EXPECT_FLOAT_EQ(*style->background_size.width, 100.0f);
    EXPECT_TRUE(style->background_size.width_is_percent);
    EXPECT_FALSE(style->background_size.height.has_value());  // auto -> aspect-preserved
}

TEST(StyleEngineTest, AuthorPaddingZeroOverridesInputUaDefault) {
    // DDG search input: `.search__input { padding: 0 }` must override the UA
    // default input padding so the input isn't taller than its search box.
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto input = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    input->set_attribute(Attr::Class, "search__input");

    std::string css = ".search__input { padding: 0; }";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, input.get());

    auto style = input->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->padding.top, 0.0f);
    EXPECT_FLOAT_EQ(style->padding.right, 0.0f);
    EXPECT_FLOAT_EQ(style->padding.bottom, 0.0f);
    EXPECT_FLOAT_EQ(style->padding.left, 0.0f);
}

TEST(StyleEngineTest, AuthorHeightAutoOverridesInputUaDefault) {
    // The submit button's `height:auto` must drop the UA default height so it can
    // stretch between top:0/bottom:0 (DDG results-page green button had a gap).
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto input = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    input->set_attribute(Attr::Type, "submit");

    std::string css = "input { height: auto; width: auto; }";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, input.get());

    auto style = input->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FALSE(style->height.has_value());  // auto cleared the UA default
    EXPECT_FALSE(style->width.has_value());
}

TEST(StyleEngineTest, ParsesAutoLengthBackgroundSize) {
    // DDG results-page logo: `background-size: auto 36px` fixes the height and
    // derives the width from the image aspect. The `auto` first token must NOT
    // short-circuit to intrinsic-size "auto".
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { background-size: auto 36px; }";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->background_size.type, ComputedStyle::BackgroundSize::Type::Length);
    EXPECT_FALSE(style->background_size.width.has_value());  // auto -> aspect-preserved
    ASSERT_TRUE(style->background_size.height.has_value());
    EXPECT_FLOAT_EQ(*style->background_size.height, 36.0f);
    EXPECT_FALSE(style->background_size.height_is_percent);
}

TEST(StyleEngineTest, SingleAutoBackgroundSizeStaysIntrinsic) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { background-size: auto; }";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    // The single-token form remains intrinsic-size Auto.
    EXPECT_EQ(style->background_size.type, ComputedStyle::BackgroundSize::Type::Auto);
    EXPECT_FALSE(style->background_size.width.has_value());
    EXPECT_FALSE(style->background_size.height.has_value());
}

TEST(StyleEngineTest, ParsesPercentBackgroundPosition) {
    // DDG magnifier: `background-position: 50% 50%` must keep the percentage so
    // the painter centers it, rather than treating 50 as a pixel offset.
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    std::string css = "div { background-position: 50% 50%; }";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->background_position.offset_x.has_value());
    EXPECT_FLOAT_EQ(*style->background_position.offset_x, 50.0f);
    EXPECT_TRUE(style->background_position.offset_x_is_percent);
    ASSERT_TRUE(style->background_position.offset_y.has_value());
    EXPECT_TRUE(style->background_position.offset_y_is_percent);
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
    EXPECT_FLOAT_EQ(img_style->width->px, 120.0f);
    EXPECT_FLOAT_EQ(img_style->height->px, 80.0f);
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

TEST(StyleEngineTest, InputSizeAttributeDoesNotChangeFontSize) {
    // `size` on <input> is the field's width in characters, not a <font>-style
    // font size. HN's `<input size="20">` was mapping through the 1..7 table to
    // 48px and overflowing the field; it must leave the font size at the default.
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto input = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    input->set_attribute(Attr::Type, "text");
    input->set_attribute(Attr::Size, "20");

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, input.get());

    auto style = input->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_FLOAT_EQ(style->font_size, 16.0f);
}

TEST(StyleEngineTest, BoldAndItalicPresentationalTags) {
    // <b>/<i> are the presentational siblings of <strong>/<em>. HN's masthead
    // uses `<b class="hnname">Hacker News</b>`, which must render bold.
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto b = DomFactory::create_element(arena, Hummingbird::Html::TagNames::B);
    auto i = DomFactory::create_element(arena, Hummingbird::Html::TagNames::I);

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, b.get());
    engine.apply(empty_sheet, i.get());

    auto b_style = b->get_computed_style();
    auto i_style = i->get_computed_style();
    ASSERT_TRUE(b_style);
    ASSERT_TRUE(i_style);
    EXPECT_EQ(b_style->weight, ComputedStyle::FontWeight::Bold);
    EXPECT_EQ(i_style->style, ComputedStyle::FontStyle::Italic);
}

TEST(StyleEngineTest, BgColorAttributeAppliesToTableCells) {
    // HN's orange header bar is `<td bgcolor="#ff6600">`; bgcolor is a legacy
    // attribute of the table elements, not just <body>.
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto td = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Td);
    td->set_attribute(Attr::BgColor, "#ff6600");

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, td.get());

    auto style = td->get_computed_style();
    ASSERT_TRUE(style);
    ASSERT_TRUE(style->background.has_value());
    EXPECT_EQ(style->background->r, 0xff);
    EXPECT_EQ(style->background->g, 0x66);
    EXPECT_EQ(style->background->b, 0x00);
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

TEST(StyleEngineTest, AuthorBackgroundOverridesCodeAndPreDefaults) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto code = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Code);
    auto pre = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Pre);
    root->append_child(std::move(code));
    root->append_child(std::move(pre));

    std::string css = "code { background-color: #eeeeee; } pre { background-color: #dddddd; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto code_style = dynamic_cast<Element*>(root->get_children()[0].get())->get_computed_style();
    auto pre_style = dynamic_cast<Element*>(root->get_children()[1].get())->get_computed_style();
    ASSERT_TRUE(code_style);
    ASSERT_TRUE(pre_style);
    ASSERT_TRUE(code_style->background.has_value());
    ASSERT_TRUE(pre_style->background.has_value());
    EXPECT_EQ(code_style->background->r, 238);
    EXPECT_EQ(code_style->background->g, 238);
    EXPECT_EQ(code_style->background->b, 238);
    EXPECT_EQ(pre_style->background->r, 221);
    EXPECT_EQ(pre_style->background->g, 221);
    EXPECT_EQ(pre_style->background->b, 221);
}

TEST(StyleEngineTest, EvaluatesMediaConditionsAgainstViewport) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto body = Hummingbird::DOM::DomFactory::create_element(arena, "body");
    auto div = Hummingbird::DOM::DomFactory::create_element(arena, "div");
    div->set_attribute("id", "target");
    body->append_child(std::move(div));

    Hummingbird::Css::Parser parser(R"(
        #target { width: 10px; }
        @media (min-width: 800px) { #target { width: 500px; } }
    )");
    auto sheet = parser.parse();

    // Wide viewport: the @media rule wins.
    {
        Hummingbird::Css::StyleEngine engine;
        engine.apply(sheet, body.get(), {1024.0f, 768.0f});
        auto style = body->get_children()[0]->get_computed_style();
        ASSERT_NE(style, nullptr);
        ASSERT_TRUE(style->width.has_value());
        EXPECT_FLOAT_EQ(style->width->px, 500.0f);
    }
    // Narrow viewport: conditioned rule filtered out.
    {
        Hummingbird::Css::StyleEngine engine;
        engine.apply(sheet, body.get(), {500.0f, 700.0f});
        auto style = body->get_children()[0]->get_computed_style();
        ASSERT_NE(style, nullptr);
        ASSERT_TRUE(style->width.has_value());
        EXPECT_FLOAT_EQ(style->width->px, 10.0f);
    }
    // Default context (no viewport): min-width conditions do not match.
    {
        Hummingbird::Css::StyleEngine engine;
        engine.apply(sheet, body.get());
        auto style = body->get_children()[0]->get_computed_style();
        ASSERT_NE(style, nullptr);
        ASSERT_TRUE(style->width.has_value());
        EXPECT_FLOAT_EQ(style->width->px, 10.0f);
    }
}
