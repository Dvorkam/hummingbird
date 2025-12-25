#include <gtest/gtest.h>

#include "TestGraphicsContext.h"
#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "layout/RenderListItem.h"
#include "layout/TreeBuilder.h"
#include "style/StyleEngine.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;

TEST(ListItemLayoutTest, GeneratesMarkerLeftOfContent) {
    ArenaAllocator arena(4096);
    auto body = make_arena_ptr<Element>(arena, "body");
    auto ul = make_arena_ptr<Element>(arena, "ul");
    auto li = make_arena_ptr<Element>(arena, "li");
    li->append_child(make_arena_ptr<Text>(arena, "Item"));
    ul->append_child(std::move(li));
    body->append_child(std::move(ul));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 1u);

    TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);

    const auto& ul_box = render_root->get_children()[0];
    ASSERT_EQ(ul_box->get_children().size(), 1u);
    auto* list_item = dynamic_cast<RenderListItem*>(ul_box->get_children()[0].get());
    ASSERT_NE(list_item, nullptr);

    const auto& marker = list_item->marker_rect();
    ASSERT_GT(marker.width, 0.0f);

    const auto& text_rect = list_item->get_children()[0]->get_rect();
    EXPECT_LT(marker.x + marker.width, text_rect.x);
}

TEST(ListItemLayoutTest, InlineThenBlockAdvancesCursor) {
    ArenaAllocator arena(4096);
    auto body = make_arena_ptr<Element>(arena, "body");
    auto ul = make_arena_ptr<Element>(arena, "ul");
    auto li = make_arena_ptr<Element>(arena, "li");
    li->append_child(make_arena_ptr<Text>(arena, "Item"));
    auto div = make_arena_ptr<Element>(arena, "div");
    div->append_child(make_arena_ptr<Text>(arena, "Block"));
    li->append_child(std::move(div));
    ul->append_child(std::move(li));
    body->append_child(std::move(ul));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 1u);

    TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);

    const auto& ul_box = render_root->get_children()[0];
    ASSERT_EQ(ul_box->get_children().size(), 1u);
    auto* list_item = dynamic_cast<RenderListItem*>(ul_box->get_children()[0].get());
    ASSERT_NE(list_item, nullptr);
    ASSERT_GE(list_item->get_children().size(), 2u);

    const auto& text_rect = list_item->get_children()[0]->get_rect();
    const auto& block_rect = list_item->get_children()[1]->get_rect();
    EXPECT_GT(block_rect.y, text_rect.y);
}
