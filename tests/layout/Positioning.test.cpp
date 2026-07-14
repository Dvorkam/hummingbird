#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "layout/TreeBuilder.h"
#include "layout/geometry/PositioningUtils.h"
#include "style/compute/StyleEngine.h"
#include "style/parser/CssParser.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Layout;

namespace {
RenderObject* find_by_id(RenderObject* node, std::string_view id) {
    if (!node) return nullptr;
    if (auto* element = dynamic_cast<const Element*>(node->get_dom_node())) {
        if (const auto* attr = element->find_attribute("id")) {
            if (*attr == id) {
                return node;
            }
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* found = find_by_id(child.get(), id)) {
            return found;
        }
    }
    return nullptr;
}
}  // namespace

TEST(PositioningLayoutTest, PositionsAbsoluteChildrenRelativeToContainingBlock) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto container = DomFactory::create_element(arena, "div");
    container->set_attribute("id", "container");
    auto flow = DomFactory::create_element(arena, "div");
    flow->set_attribute("id", "flow");
    auto abs = DomFactory::create_element(arena, "div");
    abs->set_attribute("id", "abs");
    container->append_child(std::move(flow));
    container->append_child(std::move(abs));
    body->append_child(std::move(container));

    std::string css = R"(
        #container { position: relative; width: 200px; margin: 0; padding: 0; }
        #flow { height: 20px; margin: 0; padding: 0; }
        #abs { position: absolute; top: 5px; left: 10px; width: 50px; height: 10px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 300, 200};
    render_root->layout(context, viewport);
    Positioning::apply_positioning(*render_root, context, viewport);

    auto* container_render = find_by_id(render_root.get(), "container");
    auto* flow_render = find_by_id(render_root.get(), "flow");
    auto* abs_render = find_by_id(render_root.get(), "abs");
    ASSERT_NE(container_render, nullptr);
    ASSERT_NE(flow_render, nullptr);
    ASSERT_NE(abs_render, nullptr);

    EXPECT_FLOAT_EQ(abs_render->get_rect().x, 10.0f);
    EXPECT_FLOAT_EQ(abs_render->get_rect().y, 5.0f);
    EXPECT_FLOAT_EQ(container_render->get_rect().height, flow_render->get_rect().height);
}

TEST(PositioningLayoutTest, AutoMarginsCenterBoxBetweenOpposingInsets) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto container = DomFactory::create_element(arena, "div");
    container->set_attribute("id", "container");
    auto dot = DomFactory::create_element(arena, "div");
    dot->set_attribute("id", "dot");
    container->append_child(std::move(dot));
    body->append_child(std::move(container));

    // top:0;bottom:0;left:0;right:0;margin:auto centers in both axes.
    std::string css = R"(
        #container { position: relative; width: 200px; height: 100px; margin: 0; padding: 0; }
        #dot { position: absolute; top: 0; bottom: 0; left: 0; right: 0; margin: auto;
               width: 40px; height: 20px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 300, 200};
    render_root->layout(context, viewport);
    Positioning::apply_positioning(*render_root, context, viewport);

    auto* dot_render = find_by_id(render_root.get(), "dot");
    ASSERT_NE(dot_render, nullptr);
    EXPECT_FLOAT_EQ(dot_render->get_rect().x, 80.0f);  // (200 - 40) / 2
    EXPECT_FLOAT_EQ(dot_render->get_rect().y, 40.0f);  // (100 - 20) / 2
}

TEST(PositioningLayoutTest, AutoMarginVerticallyCentersRightAlignedButton) {
    // DDG search button: top:0;bottom:0;right:2px;left:auto;margin:auto ->
    // vertically centered, pinned near the right edge.
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto container = DomFactory::create_element(arena, "div");
    container->set_attribute("id", "container");
    auto btn = DomFactory::create_element(arena, "div");
    btn->set_attribute("id", "btn");
    container->append_child(std::move(btn));
    body->append_child(std::move(container));

    std::string css = R"(
        #container { position: relative; width: 200px; height: 40px; margin: 0; padding: 0; }
        #btn { position: absolute; top: 0; bottom: 0; right: 2px; left: auto; margin: auto;
               width: 24px; height: 20px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 300, 200};
    render_root->layout(context, viewport);
    Positioning::apply_positioning(*render_root, context, viewport);

    auto* btn_render = find_by_id(render_root.get(), "btn");
    ASSERT_NE(btn_render, nullptr);
    EXPECT_FLOAT_EQ(btn_render->get_rect().x, 174.0f);  // 200 - 2 - 24 (right-aligned, left:auto)
    EXPECT_FLOAT_EQ(btn_render->get_rect().y, 10.0f);   // (40 - 20) / 2 (vertically centered)
}

TEST(PositioningLayoutTest, RelativeOffsetsShiftVisualRectWithoutAffectingFlow) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto rel = DomFactory::create_element(arena, "div");
    rel->set_attribute("id", "rel");
    body->append_child(std::move(rel));

    std::string css = R"(
        #rel { position: relative; top: 6px; left: 12px; width: 40px; height: 10px; margin: 0; padding: 0; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);
    Positioning::apply_positioning(*render_root, context, viewport);

    auto* rel_render = find_by_id(render_root.get(), "rel");
    ASSERT_NE(rel_render, nullptr);
    EXPECT_FLOAT_EQ(rel_render->get_rect().x, 12.0f);
    EXPECT_FLOAT_EQ(rel_render->get_rect().y, 6.0f);
}

TEST(PositioningLayoutTest, ZIndexOrdersTraversal) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto container = DomFactory::create_element(arena, "div");
    auto low = DomFactory::create_element(arena, "div");
    low->set_attribute("id", "low");
    auto high = DomFactory::create_element(arena, "div");
    high->set_attribute("id", "high");
    container->append_child(std::move(low));
    container->append_child(std::move(high));
    body->append_child(std::move(container));

    std::string css = R"(
        #low { position: absolute; top: 0; left: 0; z-index: 1; width: 10px; height: 10px; }
        #high { position: absolute; top: 0; left: 0; z-index: 5; width: 10px; height: 10px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);
    Positioning::apply_positioning(*render_root, context, viewport);

    std::vector<std::string> ids;
    Positioning::traverse_render_tree_z_order(
        *render_root, {0.0f, 0.0f}, [&](const RenderObject& node, const Rect&, const Point&) {
            if (auto* element = dynamic_cast<const Element*>(node.get_dom_node())) {
                if (const auto* attr = element->find_attribute("id")) {
                    ids.push_back(*attr);
                }
            }
            return Traversal::TraverseAction::Continue;
        });

    auto low_it = std::find(ids.begin(), ids.end(), "low");
    auto high_it = std::find(ids.begin(), ids.end(), "high");
    ASSERT_NE(low_it, ids.end());
    ASSERT_NE(high_it, ids.end());
    EXPECT_LT(low_it, high_it);
}

TEST(PositioningLayoutTest, ResolvesPercentOffsetsAgainstContainingBlock) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto abs = DomFactory::create_element(arena, "div");
    abs->set_attribute("id", "abs");
    body->append_child(std::move(abs));

    std::string css = R"(
        #abs { position: absolute; top: 25%; left: 10%; width: 20px; height: 10px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 200, 100};
    render_root->layout(context, viewport);
    Positioning::apply_positioning(*render_root, context, viewport);

    auto* abs_render = find_by_id(render_root.get(), "abs");
    ASSERT_NE(abs_render, nullptr);
    EXPECT_FLOAT_EQ(abs_render->get_rect().x, 20.0f);
    EXPECT_FLOAT_EQ(abs_render->get_rect().y, 25.0f);
}
