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
