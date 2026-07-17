#include "layout/flow/TextBox.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Text.h"
#include "style/types/ComputedStyle.h"
#include "test_utils/TestGraphicsContext.h"

using Hummingbird::Color;
using Hummingbird::IGraphicsContext;
using Hummingbird::ImageBitmap;
using Hummingbird::TextMetrics;
using Hummingbird::TextStyle;

class FontCaptureContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& /*viewport*/) override {}
    void clear(const Color& /*color*/) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& /*rect*/, const Color& /*color*/) override {}
    void draw_image(const ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override {}

    TextMetrics measure_text(const std::string& text, const TextStyle& style) override {
        last_font_path = style.font_path;
        constexpr float kAverageCharWidth = 8.0f;
        constexpr float kLineHeight = 16.0f;
        return TextMetrics{static_cast<float>(text.size()) * kAverageCharWidth, kLineHeight};
    }

    void draw_text(const std::string& /*text*/, float /*x*/, float /*y*/, const TextStyle& /*style*/) override {}

    std::string last_font_path;
};

class DrawCaptureContext : public Hummingbird::Test::TestGraphicsContext {
public:
    void draw_text(const std::string& text, float x, float y, const TextStyle& /*style*/) override {
        draws.push_back({text, x, y});
    }

    struct DrawCall {
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
    };

    std::vector<DrawCall> draws;
};

// NOTE: This test requires the font file 'assets/fonts/Roboto-Regular.ttf' to be present.
TEST(TextBoxLayoutTest, SimpleTextMeasurement) {
    // 1. Create a DOM Text node
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "Hello");

    // 2. Create a TextBox render object
    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());

    // 3. Layout the text box
    Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
    Hummingbird::Test::TestGraphicsContext context;
    text_box->layout(context, bounds);

    // 4. Assert dimensions
    // These expected values are approximate and depend on the font rendering.
    // They might need adjustment if the font or rendering backend changes.
    EXPECT_GT(text_box->get_rect().width, 25);
    EXPECT_LT(text_box->get_rect().width, 45);
    EXPECT_GT(text_box->get_rect().height, 10);
    EXPECT_LT(text_box->get_rect().height, 25);
}

TEST(TextBoxLayoutTest, CollapsesWhitespaceInNormalMode) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "Hello   \n   world");
    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
    Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
    Hummingbird::Test::TestGraphicsContext context;
    text_box->layout(context, bounds);

    EXPECT_EQ(text_box->rendered_text(), "Hello world");
}

TEST(TextBoxLayoutTest, PreservesWhitespaceInPreMode) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "Line1\n  Line2");
    Hummingbird::Css::ComputedStyle pre_style = Hummingbird::Css::default_computed_style();
    pre_style.whitespace = Hummingbird::Css::ComputedStyle::WhiteSpace::Preserve;
    // Manually attach style since StyleEngine isn't invoked in this test.
    dom_text->set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(pre_style));

    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
    Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
    Hummingbird::Test::TestGraphicsContext context;

    text_box->layout(context, bounds);
    EXPECT_EQ(text_box->rendered_text(), "Line1\n  Line2");
}

TEST(TextBoxLayoutTest, PreservedTextPaintsPerLineWithoutControlCharacters) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "Preformatted\ntext stays\naligned.");
    Hummingbird::Css::ComputedStyle pre_style = Hummingbird::Css::default_computed_style();
    pre_style.whitespace = Hummingbird::Css::ComputedStyle::WhiteSpace::Preserve;
    dom_text->set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(pre_style));

    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
    DrawCaptureContext context;

    // Drive the inline participant path used when <pre> text flows through a
    // block container (this is where the raw run text used to reach painting).
    auto* participant = static_cast<Hummingbird::Layout::IInlineParticipant*>(text_box.get());
    participant->reset_inline_layout();
    participant->measure_inline(context);
    std::vector<Hummingbird::Layout::InlineRun> runs;
    participant->collect_inline_runs(context, runs);
    ASSERT_EQ(runs.size(), 1u);
    Hummingbird::Layout::InlineFragment fragment;
    fragment.run_index = 0;
    fragment.line_index = 0;
    fragment.rect = {0.0f, 0.0f, runs[0].width, runs[0].height};
    fragment.ascent = runs[0].ascent;
    participant->apply_inline_fragment(0, fragment, runs[0]);
    participant->finalize_inline_layout();
    text_box->paint(context, {0.0f, 0.0f});

    ASSERT_EQ(context.draws.size(), 3u);
    EXPECT_EQ(context.draws[0].text, "Preformatted");
    EXPECT_EQ(context.draws[1].text, "text stays");
    EXPECT_EQ(context.draws[2].text, "aligned.");
    for (const auto& draw : context.draws) {
        EXPECT_EQ(draw.text.find('\n'), std::string::npos);
        EXPECT_EQ(draw.text.find('\r'), std::string::npos);
    }
    // Lines stack vertically.
    EXPECT_LT(context.draws[0].y, context.draws[1].y);
    EXPECT_LT(context.draws[1].y, context.draws[2].y);
}

TEST(TextBoxLayoutTest, SelectsFontByBoldItalicCombination) {
    auto run_case = [](Hummingbird::Css::ComputedStyle style, std::string_view expected_suffix) {
        Hummingbird::Core::ArenaAllocator arena(1024);
        auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "Hello");
        dom_text->set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(style));

        auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
        Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
        FontCaptureContext context;

        text_box->layout(context, bounds);

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

TEST(TextBoxLayoutTest, IncludesPaddingAndBorderInSize) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "Hi");
    auto style = Hummingbird::Css::default_computed_style();
    style.padding.left = 2.0f;
    style.padding.right = 2.0f;
    style.border_width.left = 1.0f;
    style.border_width.right = 1.0f;
    dom_text->set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(style));

    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
    Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
    Hummingbird::Test::TestGraphicsContext context;

    text_box->layout(context, bounds);

    const auto& rect = text_box->get_rect();
    EXPECT_FLOAT_EQ(rect.width, 16.0f + 6.0f);
}

TEST(TextBoxLayoutTest, AppliesTextTransformDuringLayout) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "mixed Case");
    auto style = Hummingbird::Css::default_computed_style();
    style.text_transform = Hummingbird::Css::ComputedStyle::TextTransform::Uppercase;
    dom_text->set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(style));

    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
    Hummingbird::Layout::Rect bounds = {0, 0, 800, 600};
    Hummingbird::Test::TestGraphicsContext context;
    text_box->layout(context, bounds);

    EXPECT_EQ(text_box->rendered_text(), "MIXED CASE");
}

TEST(TextBoxLayoutTest, UsesTextOverflowEllipsisForNoWrap) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "This is a long single-line sentence.");
    auto style = Hummingbird::Css::default_computed_style();
    style.whitespace = Hummingbird::Css::ComputedStyle::WhiteSpace::NoWrap;
    style.text_overflow = Hummingbird::Css::ComputedStyle::TextOverflow::Ellipsis;
    dom_text->set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(style));

    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
    DrawCaptureContext context;
    text_box->layout(context, {0, 0, 80, 40});
    text_box->paint_self(context, {0.0f, 0.0f});

    ASSERT_FALSE(context.draws.empty());
    const auto& drawn = context.draws.back().text;
    EXPECT_NE(drawn.find("..."), std::string::npos);
}

TEST(TextBoxLayoutTest, BreakWordWrapIncreasesLineCountForLongWords) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "SupercalifragilisticexpialidociousWord");
    auto style = Hummingbird::Css::default_computed_style();
    style.word_wrap = Hummingbird::Css::ComputedStyle::WordWrap::BreakWord;
    dom_text->set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(style));

    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
    Hummingbird::Test::TestGraphicsContext context;
    text_box->layout(context, {0, 0, 80, 400});

    EXPECT_GT(text_box->get_rect().height, style.font_size);
}

TEST(TextBoxLayoutTest, AppliesTextIndentToFirstLinePaint) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto dom_text = Hummingbird::DOM::DomFactory::create_text(arena, "Indent me");
    auto style = Hummingbird::Css::default_computed_style();
    style.text_indent = 12.0f;
    dom_text->set_computed_style(std::make_shared<Hummingbird::Css::ComputedStyle>(style));

    auto text_box = Hummingbird::Layout::TextBox::create(dom_text.get());
    DrawCaptureContext context;
    text_box->layout(context, {0, 0, 400, 80});
    text_box->paint_self(context, {0.0f, 0.0f});

    ASSERT_FALSE(context.draws.empty());
    EXPECT_GE(context.draws.front().x, 0.0f);
}
