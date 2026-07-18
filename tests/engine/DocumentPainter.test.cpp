#include "engine/document/DocumentPainter.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "engine/document/DocumentInputPainter.h"
#include "layout/block/BlockBox.h"
#include "style/types/ComputedStyle.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
using Hummingbird::Color;
using Hummingbird::Core::ArenaAllocator;
using Hummingbird::Core::ArenaPtr;
using Hummingbird::DOM::Element;
using Hummingbird::Engine::DocumentInputController;
using Hummingbird::Engine::DocumentPainter;
using Hummingbird::Layout::BlockBox;
using Hummingbird::Layout::Rect;
using Hummingbird::Test::TestGraphicsContext;

// Records fill_rect calls (with color) so tests can inspect what was painted.
class RectRecordingContext : public TestGraphicsContext {
public:
    void fill_rect(const Rect& rect, const Color& color) override { rects.emplace_back(rect, color); }
    std::vector<std::pair<Rect, Color>> rects;
};
}  // namespace

TEST(DocumentPainterTest, ReusesDisplayListWhenInputsMatch) {
    ArenaAllocator arena(1024);
    ArenaPtr<Element> root = Element::create(arena, "div");
    auto render_root = BlockBox::create(root.get());
    render_root->set_rect(Rect{0.0f, 0.0f, 200.0f, 50.0f});

    DocumentPainter painter;
    DocumentInputController input;
    TestGraphicsContext graphics;
    Rect viewport{0.0f, 0.0f, 200.0f, 50.0f};

    painter.paint(render_root.get(), graphics, viewport, false, 0.0f, input);
    size_t gen = painter.display_list_generation();
    ASSERT_GT(gen, 0u);

    painter.paint(render_root.get(), graphics, viewport, false, 0.0f, input);
    EXPECT_EQ(painter.display_list_generation(), gen);
}

TEST(DocumentInputPainterTest, FocusRingFollowsBorderRadius) {
    namespace Css = Hummingbird::Css;
    ArenaAllocator arena(1024);
    ArenaPtr<Element> input = Element::create(arena, "input");
    auto node = BlockBox::create(input.get());
    const Rect absolute{10.0f, 10.0f, 120.0f, 24.0f};
    node->set_rect(absolute);

    // A bordered, rounded input: the synthetic focus ring should hug the radius.
    auto style = std::make_shared<Css::ComputedStyle>();
    style->border_style = Css::ComputedStyle::BorderStyle::Solid;
    style->border_width = {1.0f, 1.0f, 1.0f, 1.0f};
    style->border_radius.set_all(Css::CornerRadius{8.0f, false});
    input->set_computed_style(style);  // the render object reads style from its DOM node

    RectRecordingContext graphics;
    Hummingbird::Engine::paint_input_control(*input, *node, absolute, {0.0f, 0.0f}, graphics,
                                             /*repaint_background*/ false, /*focused*/ true, /*caret*/ 0,
                                             /*scroll_y*/ 0.0f);

    // The focus ring is the blue kFocusRingColor; the caret (also drawn) is not.
    const Color ring{66, 133, 244, 255};
    const auto is_ring = [&](const Color& c) {
        return c.r == ring.r && c.g == ring.g && c.b == ring.b && c.a == ring.a;
    };

    int ring_rects = 0;
    bool covers_corner = false;
    const float corner_x = absolute.x + 0.5f;  // the extreme top-left corner pixel
    const float corner_y = absolute.y + 0.5f;
    for (const auto& [rect, color] : graphics.rects) {
        if (!is_ring(color)) continue;
        ++ring_rects;
        if (corner_x >= rect.x && corner_x < rect.x + rect.width && corner_y >= rect.y &&
            corner_y < rect.y + rect.height) {
            covers_corner = true;
        }
    }

    EXPECT_GT(ring_rects, 0);     // the ring was drawn
    EXPECT_FALSE(covers_corner);  // ...and its corner is rounded, not filled square
}

TEST(DocumentPainterTest, RebuildsDisplayListWhenInputsChange) {
    ArenaAllocator arena(1024);
    ArenaPtr<Element> root = Element::create(arena, "div");
    auto render_root = BlockBox::create(root.get());
    render_root->set_rect(Rect{0.0f, 0.0f, 200.0f, 50.0f});

    DocumentPainter painter;
    DocumentInputController input;
    TestGraphicsContext graphics;
    Rect viewport{0.0f, 0.0f, 200.0f, 50.0f};

    painter.paint(render_root.get(), graphics, viewport, false, 0.0f, input);
    size_t gen = painter.display_list_generation();
    ASSERT_GT(gen, 0u);

    painter.paint(render_root.get(), graphics, viewport, false, 10.0f, input);
    EXPECT_GT(painter.display_list_generation(), gen);
    gen = painter.display_list_generation();

    Rect new_viewport{0.0f, 0.0f, 180.0f, 50.0f};
    painter.paint(render_root.get(), graphics, new_viewport, false, 10.0f, input);
    EXPECT_GT(painter.display_list_generation(), gen);
    gen = painter.display_list_generation();

    painter.invalidate_display_list();
    painter.paint(render_root.get(), graphics, new_viewport, false, 10.0f, input);
    EXPECT_GT(painter.display_list_generation(), gen);
}
