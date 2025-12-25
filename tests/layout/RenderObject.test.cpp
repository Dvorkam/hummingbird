#include <gtest/gtest.h>

#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "layout/BlockBox.h"
#include "layout/RenderObject.h"
#include "layout/TextBox.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;

TEST(InlineLayoutAccessTest, ReportsInlineStatus) {
    Text text_node("Inline");
    TextBox text_box(&text_node);
    EXPECT_TRUE(InlineLayoutAccess::is_inline(text_box));

    Element div("div");
    BlockBox block_box(&div);
    EXPECT_FALSE(InlineLayoutAccess::is_inline(block_box));
}
