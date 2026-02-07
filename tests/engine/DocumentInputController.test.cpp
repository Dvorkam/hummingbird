#include "engine/document/DocumentInputController.h"

#include <gtest/gtest.h>

#include <memory>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "layout/block/BlockBox.h"

namespace {
using Hummingbird::InputEvent;
using Hummingbird::Key;
using Hummingbird::Core::ArenaAllocator;
using Hummingbird::Core::ArenaPtr;
using Hummingbird::DOM::Element;
using Hummingbird::Engine::DocumentInputController;
using Hummingbird::Layout::BlockBox;
using Hummingbird::Layout::Point;
using Hummingbird::Layout::Rect;

std::string utf8(const char8_t* text) {
    return std::string(reinterpret_cast<const char*>(text));
}

InputEvent make_key_event(Key key) {
    InputEvent event;
    event.type = Hummingbird::EventType::KeyDown;
    event.key.key = key;
    return event;
}

struct InputRenderTree {
    ArenaAllocator arena;
    ArenaPtr<Element> root;
    Element* input = nullptr;
    std::unique_ptr<BlockBox> render_root;

    InputRenderTree() : arena(1024) {
        root = Element::create(arena, "div");
        auto input_node = Element::create(arena, "input");
        input = input_node.get();
        root->append_child(std::move(input_node));

        render_root = BlockBox::create(root.get());
        auto input_box = BlockBox::create(input);
        input_box->set_rect(Rect{10.0f, 5.0f, 100.0f, 20.0f});
        render_root->set_rect(Rect{0.0f, 0.0f, 200.0f, 50.0f});
        render_root->append_child(std::move(input_box));
    }
};
}  // namespace

TEST(DocumentInputControllerTest, FocusesAndClearsInput) {
    InputRenderTree tree;
    DocumentInputController controller;
    Rect viewport{0.0f, 0.0f, 200.0f, 50.0f};
    Point hit{15.0f, 10.0f};

    EXPECT_TRUE(controller.focus_input_at(tree.render_root.get(), hit, viewport, 0.0f));
    EXPECT_TRUE(controller.has_focus());
    ASSERT_TRUE(controller.focused_value().has_value());
    EXPECT_EQ(*controller.focused_value(), "");

    EXPECT_TRUE(controller.clear_focus());
    EXPECT_FALSE(controller.has_focus());
    EXPECT_FALSE(controller.clear_focus());
}

TEST(DocumentInputControllerTest, EditsUtf8ValuesWithCaretMoves) {
    InputRenderTree tree;
    tree.input->set_attribute("value", utf8(u8"aá"));

    DocumentInputController controller;
    Rect viewport{0.0f, 0.0f, 200.0f, 50.0f};
    Point hit{15.0f, 10.0f};

    ASSERT_TRUE(controller.focus_input_at(tree.render_root.get(), hit, viewport, 0.0f));
    controller.handle_key_down(make_key_event(Key::Left));
    controller.handle_key_down(make_key_event(Key::Backspace));

    ASSERT_TRUE(controller.focused_value().has_value());
    EXPECT_EQ(*controller.focused_value(), utf8(u8"á"));

    controller.handle_key_down(make_key_event(Key::Home));
    controller.handle_key_down(make_key_event(Key::Delete));
    ASSERT_TRUE(controller.focused_value().has_value());
    EXPECT_EQ(*controller.focused_value(), "");
}

TEST(DocumentInputControllerTest, DeletesAreNoopsOnEmptyValue) {
    InputRenderTree tree;
    DocumentInputController controller;
    Rect viewport{0.0f, 0.0f, 200.0f, 50.0f};
    Point hit{15.0f, 10.0f};

    ASSERT_TRUE(controller.focus_input_at(tree.render_root.get(), hit, viewport, 0.0f));
    auto backspace = controller.handle_key_down(make_key_event(Key::Backspace));
    EXPECT_TRUE(backspace.handled);
    EXPECT_TRUE(backspace.needs_repaint);

    auto forward = controller.handle_key_down(make_key_event(Key::Delete));
    EXPECT_TRUE(forward.handled);
    EXPECT_TRUE(forward.needs_repaint);

    ASSERT_TRUE(controller.focused_value().has_value());
    EXPECT_EQ(*controller.focused_value(), "");
}
