#include <gtest/gtest.h>

#include "core/dom/Element.h"
#include "layout/RenderBreak.h"
#include "layout/RenderRule.h"
#include "TestGraphicsContext.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;

TEST(RenderBreakLayoutTest, UsesDefaultLineHeightWhenUnset) {
    Element br("br");
    RenderBreak render_break(&br);

    TestGraphicsContext context;
    Rect bounds{0, 0, 100, 0};
    render_break.layout(context, bounds);

    EXPECT_FLOAT_EQ(render_break.get_rect().height, 16.0f);
    EXPECT_FLOAT_EQ(render_break.get_rect().width, 0.0f);
}

TEST(RenderRuleLayoutTest, UsesDefaultHeightWhenUnset) {
    Element hr("hr");
    RenderRule render_rule(&hr);

    TestGraphicsContext context;
    Rect bounds{0, 0, 120, 0};
    render_rule.layout(context, bounds);

    EXPECT_FLOAT_EQ(render_rule.get_rect().height, 2.0f);
    EXPECT_FLOAT_EQ(render_rule.get_rect().width, 120.0f);
}
