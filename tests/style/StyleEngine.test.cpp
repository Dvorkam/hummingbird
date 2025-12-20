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

    // Build a small DOM tree to traverse.
    auto root = make_arena_ptr<Element>(arena, "div");
    root->append_child(std::move(ul));
    root->append_child(std::move(pre));
    root->append_child(std::move(anchor));

    StyleEngine engine;
    Stylesheet empty_sheet;
    engine.apply(empty_sheet, root.get());

    auto ul_style = dynamic_cast<Element*>(root->get_children()[0].get())->get_computed_style();
    auto pre_style = dynamic_cast<Element*>(root->get_children()[1].get())->get_computed_style();
    auto a_style = dynamic_cast<Element*>(root->get_children()[2].get())->get_computed_style();

    ASSERT_TRUE(ul_style);
    ASSERT_TRUE(pre_style);
    ASSERT_TRUE(a_style);

    EXPECT_FLOAT_EQ(ul_style->padding.left, 20.0f);
    EXPECT_EQ(pre_style->whitespace, ComputedStyle::WhiteSpace::Preserve);
    EXPECT_TRUE(pre_style->font_monospace);

    EXPECT_EQ(a_style->color.r, 0);
    EXPECT_EQ(a_style->color.g, 0);
    EXPECT_EQ(a_style->color.b, 255);
    EXPECT_TRUE(a_style->underline);
}
