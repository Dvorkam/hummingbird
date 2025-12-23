#include <gtest/gtest.h>

#include <string>
#include "layout/TextBox.h"
#include "core/dom/Text.h"
#include "style/ComputedStyle.h"
#include "TestGraphicsContext.h"

class FontCaptureContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& /*viewport*/) override {}
    void clear(const Color& /*color*/) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& /*rect*/, const Color& /*color*/) override {}

    TextMetrics measure_text(const std::string& text, const TextStyle& style) override {
        last_font_path = style.font_path;
        constexpr float kAverageCharWidth = 8.0f;
        constexpr float kLineHeight = 16.0f;
        return TextMetrics{static_cast<float>(text.size()) * kAverageCharWidth, kLineHeight};
    }

    void draw_text(const std::string& /*text*/, float /*x*/, float /*y*/, const TextStyle& /*style*/) override {}

    std::string last_font_path;
};

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

TEST(TextBoxLayoutTest, SelectsFontByBoldItalicCombination) {
    auto run_case = [](Hummingbird::Css::ComputedStyle style, std::string_view expected_suffix) {
        Hummingbird::DOM::Text dom_text("Hello");
        dom_text.set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(style));

        Hummingbird::Layout::TextBox text_box(&dom_text);
        Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
        FontCaptureContext context;

        text_box.layout(context, bounds);

        ASSERT_NE(context.last_font_path.find(expected_suffix), std::string::npos);
    };

    auto base = Hummingbird::Css::default_computed_style();

    run_case(base, "Roboto-Regular.ttf");

    auto bold = base;
    bold.weight = Hummingbird::Css::ComputedStyle::FontWeight::Bold;
    run_case(bold, "Roboto-Bold.ttf");

    auto italic = base;
    italic.style = Hummingbird::Css::ComputedStyle::FontStyle::Italic;
    run_case(italic, "Roboto-Italic.ttf");

    auto bold_italic = base;
    bold_italic.weight = Hummingbird::Css::ComputedStyle::FontWeight::Bold;
    bold_italic.style = Hummingbird::Css::ComputedStyle::FontStyle::Italic;
    run_case(bold_italic, "Roboto-BoldItalic.ttf");
}
