#include <gtest/gtest.h>
#include "style/Parser.h"
#include "style/StyleEngine.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/ArenaAllocator.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;

TEST(StyleEngineTest, AppliesRulesAndCascade) {
    // DOM: <div class="box" id="main"><span></span></div>
    ArenaAllocator arena(2048);
    auto root = make_arena_ptr<Element>(arena, "div");
    root->set_attribute("class", "box");
    root->set_attribute("id", "main");
    root->append_child(make_arena_ptr<Element>(arena, "span"));

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
    auto ul = make_arena_ptr<Element>(arena, "ul");
    auto pre = make_arena_ptr<Element>(arena, "pre");
    auto anchor = make_arena_ptr<Element>(arena, "a");
    auto code = make_arena_ptr<Element>(arena, "code");
    auto blockquote = make_arena_ptr<Element>(arena, "blockquote");
    auto hr = make_arena_ptr<Element>(arena, "hr");
    auto h1 = make_arena_ptr<Element>(arena, "h1");

    // Build a small DOM tree to traverse.
    auto root = make_arena_ptr<Element>(arena, "div");
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
    auto root = make_arena_ptr<Element>(arena, "p");
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

TEST(StyleEngineTest, LaterRuleWinsOnEqualSpecificity) {
    ArenaAllocator arena(2048);
    auto root = make_arena_ptr<Element>(arena, "div");
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
