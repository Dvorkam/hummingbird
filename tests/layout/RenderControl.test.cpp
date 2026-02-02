#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "layout/controls/RenderBreak.h"
#include "layout/controls/RenderRule.h"
#include "style/compute/ComputedStyle.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;

TEST(RenderBreakLayoutTest, UsesDefaultLineHeightWhenUnset) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto br = DomFactory::create_element(arena, "br");
    auto render_break = RenderBreak::create(br.get());

    Hummingbird::Test::TestGraphicsContext context;
    Rect bounds{0, 0, 100, 0};
    render_break->layout(context, bounds);

    EXPECT_FLOAT_EQ(render_break->get_rect().height, 16.0f);
    EXPECT_FLOAT_EQ(render_break->get_rect().width, 0.0f);
}

TEST(RenderRuleLayoutTest, UsesDefaultHeightWhenUnset) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto hr = DomFactory::create_element(arena, "hr");
    auto render_rule = RenderRule::create(hr.get());

    Hummingbird::Test::TestGraphicsContext context;
    Rect bounds{0, 0, 120, 0};
    render_rule->layout(context, bounds);

    EXPECT_FLOAT_EQ(render_rule->get_rect().height, 2.0f);
    EXPECT_FLOAT_EQ(render_rule->get_rect().width, 120.0f);
}

TEST(RenderRuleLayoutTest, HonorsWidthAndBorderWhenNoHeight) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto hr = DomFactory::create_element(arena, "hr");
    auto style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
    style->width = 50.0f;
    style->border_style = Hummingbird::Css::ComputedStyle::BorderStyle::Solid;
    style->border_width = {1.0f, 0.0f, 3.0f, 0.0f};
    hr->set_computed_style(style);

    auto render_rule = RenderRule::create(hr.get());
    Hummingbird::Test::TestGraphicsContext context;
    Rect bounds{0, 0, 120, 0};
    render_rule->layout(context, bounds);

    EXPECT_FLOAT_EQ(render_rule->get_rect().height, 4.0f);
    EXPECT_FLOAT_EQ(render_rule->get_rect().width, 50.0f);
}
