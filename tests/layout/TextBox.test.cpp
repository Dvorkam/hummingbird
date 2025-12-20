#include <gtest/gtest.h>
#include "layout/TextBox.h"
#include "core/dom/Text.h"
#include "style/ComputedStyle.h"
#include "TestGraphicsContext.h"

// NOTE: This test requires the font file 'assets/fonts/Roboto-Regular.ttf' to be present.
TEST(TextBoxLayoutTest, SimpleTextMeasurement) {
    // 1. Create a DOM Text node
    Hummingbird::DOM::Text dom_text("Hello");

    // 2. Create a TextBox render object
    Hummingbird::Layout::TextBox text_box(&dom_text);

    // 3. Layout the text box
    Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
    TestGraphicsContext context;
    text_box.layout(context, bounds);

    // 4. Assert dimensions
    // These expected values are approximate and depend on the font rendering.
    // They might need adjustment if the font or rendering backend changes.
    EXPECT_GT(text_box.get_rect().width, 25);
    EXPECT_LT(text_box.get_rect().width, 45);
    EXPECT_GT(text_box.get_rect().height, 10);
    EXPECT_LT(text_box.get_rect().height, 25);
}

TEST(TextBoxLayoutTest, CollapsesWhitespaceInNormalMode) {
    Hummingbird::DOM::Text dom_text("Hello   \n   world");
    Hummingbird::Layout::TextBox text_box(&dom_text);
    Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
    TestGraphicsContext context;
    text_box.layout(context, bounds);

    EXPECT_EQ(text_box.rendered_text(), "Hello world");
}

TEST(TextBoxLayoutTest, PreservesWhitespaceInPreMode) {
    Hummingbird::DOM::Text dom_text("Line1\n  Line2");
    Hummingbird::Css::ComputedStyle pre_style = Hummingbird::Css::default_computed_style();
    pre_style.whitespace = Hummingbird::Css::ComputedStyle::WhiteSpace::Preserve;
    // Manually attach style since StyleEngine isn't invoked in this test.
    dom_text.set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(pre_style));

    Hummingbird::Layout::TextBox text_box(&dom_text);
    Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
    TestGraphicsContext context;

    text_box.layout(context, bounds);
    EXPECT_EQ(text_box.rendered_text(), "Line1\n  Line2");
}
