#include "engine/document/DocumentPainter.h"

#include <gtest/gtest.h>

#include <memory>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "layout/block/BlockBox.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
using Hummingbird::Core::ArenaAllocator;
using Hummingbird::Core::ArenaPtr;
using Hummingbird::DOM::Element;
using Hummingbird::Engine::DocumentInputController;
using Hummingbird::Engine::DocumentPainter;
using Hummingbird::Layout::BlockBox;
using Hummingbird::Layout::Rect;
using Hummingbird::Test::TestGraphicsContext;
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
