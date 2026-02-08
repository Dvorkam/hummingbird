#include <gtest/gtest.h>

#include <memory>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "layout/block/BlockBox.h"
#include "layout/flow/TextBox.h"
#include "style/compute/ComputedStyle.h"

namespace {
using Hummingbird::Core::ArenaAllocator;
using Hummingbird::Core::ArenaPtr;
using Hummingbird::Css::ComputedStyle;
using Hummingbird::DOM::Element;
using Hummingbird::DOM::Text;
using Hummingbird::Layout::BlockBox;
using Hummingbird::Layout::TextBox;
}  // namespace

TEST(RenderObjectTest, InlineRefReportsInlineParticipants) {
    ArenaAllocator arena(1024);
    ArenaPtr<Text> text_node = Text::create(arena, "Inline");
    auto text_box = TextBox::create(text_node.get());
    EXPECT_TRUE(text_box->Inline());

    ArenaPtr<Element> div = Element::create(arena, "div");
    auto block_box = BlockBox::create(div.get());
    EXPECT_FALSE(block_box->Inline());
}

TEST(RenderObjectTest, InlineRefSkipsAbsolutelyPositionedInline) {
    ArenaAllocator arena(1024);
    ArenaPtr<Text> text_node = Text::create(arena, "Inline");
    auto text_box = TextBox::create(text_node.get());
    auto style = std::make_shared<ComputedStyle>(Hummingbird::Css::default_computed_style());
    style->position = ComputedStyle::Position::Absolute;
    text_node->set_computed_style(style);

    EXPECT_FALSE(text_box->Inline());
}
