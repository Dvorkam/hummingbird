#include <gtest/gtest.h>

#include "TestGraphicsContext.h"
#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "layout/RenderBreak.h"
#include "layout/RenderRule.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;

TEST(RenderBreakLayoutTest, UsesDefaultLineHeightWhenUnset) {
    ArenaAllocator arena(1024);
    auto br = DomFactory::create_element(arena, "br");
    auto render_break = RenderBreak::create(br.get());

    TestGraphicsContext context;
    Rect bounds{0, 0, 100, 0};
    render_break->layout(context, bounds);

    EXPECT_FLOAT_EQ(render_break->get_rect().height, 16.0f);
    EXPECT_FLOAT_EQ(render_break->get_rect().width, 0.0f);
}

TEST(RenderRuleLayoutTest, UsesDefaultHeightWhenUnset) {
    ArenaAllocator arena(1024);
    auto hr = DomFactory::create_element(arena, "hr");
    auto render_rule = RenderRule::create(hr.get());

    TestGraphicsContext context;
    Rect bounds{0, 0, 120, 0};
    render_rule->layout(context, bounds);

    EXPECT_FLOAT_EQ(render_rule->get_rect().height, 2.0f);
    EXPECT_FLOAT_EQ(render_rule->get_rect().width, 120.0f);
}
