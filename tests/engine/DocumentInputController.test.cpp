#include "engine/document/DocumentInputController.h"

#include <gtest/gtest.h>

#include <initializer_list>
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

std::string utf8_bytes(std::initializer_list<unsigned char> bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.begin()), bytes.size());
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

struct OverlappingInputRenderTree {
    ArenaAllocator arena;
    ArenaPtr<Element> root;
    Element* text_input = nullptr;
    Element* submit_input = nullptr;
    std::unique_ptr<BlockBox> render_root;

    OverlappingInputRenderTree() : arena(2048) {
        root = Element::create(arena, "div");

        auto text_input_node = Element::create(arena, "input");
        text_input = text_input_node.get();
        text_input->set_attribute("type", "text");
        root->append_child(std::move(text_input_node));

        auto submit_input_node = Element::create(arena, "input");
        submit_input = submit_input_node.get();
        submit_input->set_attribute("type", "submit");
        root->append_child(std::move(submit_input_node));

        render_root = BlockBox::create(root.get());

        auto text_input_box = BlockBox::create(text_input);
        text_input_box->set_rect(Rect{10.0f, 5.0f, 140.0f, 20.0f});
        render_root->append_child(std::move(text_input_box));

        auto submit_input_box = BlockBox::create(submit_input);
        submit_input_box->set_rect(Rect{10.0f, 5.0f, 140.0f, 20.0f});
        render_root->append_child(std::move(submit_input_box));

        render_root->set_rect(Rect{0.0f, 0.0f, 200.0f, 60.0f});
    }
};

struct InputButtonRenderTree {
    ArenaAllocator arena;
    ArenaPtr<Element> root;
    Element* button = nullptr;
    std::unique_ptr<BlockBox> render_root;

    InputButtonRenderTree() : arena(2048) {
        root = Element::create(arena, "div");
        auto button_node = Element::create(arena, "button");
        button = button_node.get();
        root->append_child(std::move(button_node));

        render_root = BlockBox::create(root.get());
        auto button_box = BlockBox::create(button);
        button_box->set_rect(Rect{20.0f, 8.0f, 80.0f, 22.0f});
        render_root->set_rect(Rect{0.0f, 0.0f, 200.0f, 60.0f});
        render_root->append_child(std::move(button_box));
    }
};

struct TextareaRenderTree {
    ArenaAllocator arena;
    ArenaPtr<Element> root;
    Element* textarea = nullptr;
    std::unique_ptr<BlockBox> render_root;

    TextareaRenderTree() : arena(1024) {
        root = Element::create(arena, "div");
        auto textarea_node = Element::create(arena, "textarea");
        textarea = textarea_node.get();
        root->append_child(std::move(textarea_node));

        render_root = BlockBox::create(root.get());
        auto textarea_box = BlockBox::create(textarea);
        textarea_box->set_rect(Rect{10.0f, 5.0f, 160.0f, 64.0f});
        render_root->set_rect(Rect{0.0f, 0.0f, 200.0f, 80.0f});
        render_root->append_child(std::move(textarea_box));
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
    const std::string a_acute = utf8_bytes({0xC3, 0xA1});
    tree.input->set_attribute("value", std::string("a") + a_acute);

    DocumentInputController controller;
    Rect viewport{0.0f, 0.0f, 200.0f, 50.0f};
    Point hit{15.0f, 10.0f};

    ASSERT_TRUE(controller.focus_input_at(tree.render_root.get(), hit, viewport, 0.0f));
    controller.handle_key_down(make_key_event(Key::Left));
    controller.handle_key_down(make_key_event(Key::Backspace));

    ASSERT_TRUE(controller.focused_value().has_value());
    EXPECT_EQ(*controller.focused_value(), a_acute);

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

TEST(DocumentInputControllerTest, PrefersEditableTextInputOverSubmitInputAtSamePoint) {
    OverlappingInputRenderTree tree;
    DocumentInputController controller;
    Rect viewport{0.0f, 0.0f, 200.0f, 60.0f};
    Point hit{15.0f, 10.0f};

    ASSERT_TRUE(controller.focus_input_at(tree.render_root.get(), hit, viewport, 0.0f));
    auto text_result = controller.handle_text_input("abc");
    EXPECT_TRUE(text_result.handled);
    ASSERT_TRUE(controller.focused_value().has_value());
    EXPECT_EQ(*controller.focused_value(), "abc");

    const auto* text_value = tree.text_input->find_attribute("value");
    ASSERT_NE(text_value, nullptr);
    EXPECT_EQ(*text_value, "abc");
    EXPECT_EQ(tree.submit_input->find_attribute("value"), nullptr);
}

TEST(DocumentInputControllerTest, FocusesAutofocusInputOnLoadPath) {
    InputRenderTree tree;
    tree.input->set_attribute("autofocus", "");

    DocumentInputController controller;
    EXPECT_TRUE(controller.focus_autofocus_input(tree.render_root.get()));
    EXPECT_TRUE(controller.has_focus());

    auto edit = controller.handle_text_input("ddg");
    EXPECT_TRUE(edit.handled);
    ASSERT_TRUE(controller.focused_value().has_value());
    EXPECT_EQ(*controller.focused_value(), "ddg");
}

TEST(DocumentInputControllerTest, FocusStateTracksFocusedInputPseudoClass) {
    InputRenderTree tree;
    DocumentInputController controller;
    Rect viewport{0.0f, 0.0f, 200.0f, 50.0f};
    Point hit{15.0f, 10.0f};

    EXPECT_FALSE(tree.input->has_pseudo_state(Element::PseudoState::Focus));
    ASSERT_TRUE(controller.focus_input_at(tree.render_root.get(), hit, viewport, 0.0f));
    EXPECT_TRUE(tree.input->has_pseudo_state(Element::PseudoState::Focus));
    EXPECT_TRUE(controller.clear_focus());
    EXPECT_FALSE(tree.input->has_pseudo_state(Element::PseudoState::Focus));
}

TEST(DocumentInputControllerTest, TextareaEnterAddsNewlineInsteadOfSubmitting) {
    TextareaRenderTree tree;
    DocumentInputController controller;
    Rect viewport{0.0f, 0.0f, 200.0f, 80.0f};
    Point hit{15.0f, 10.0f};

    ASSERT_TRUE(controller.focus_input_at(tree.render_root.get(), hit, viewport, 0.0f));
    EXPECT_TRUE(controller.focused_is_multiline());
    EXPECT_TRUE(controller.handle_text_input("first line").handled);

    auto enter = controller.handle_key_down(make_key_event(Key::Enter));
    EXPECT_TRUE(enter.handled);
    EXPECT_TRUE(enter.needs_repaint);
    EXPECT_TRUE(controller.handle_text_input("second line").handled);

    ASSERT_TRUE(controller.focused_value().has_value());
    EXPECT_EQ(*controller.focused_value(), "first line\nsecond line");
}

TEST(DocumentInputControllerTest, InteractionStateTracksHoverAndActiveOnButton) {
    InputButtonRenderTree tree;
    DocumentInputController controller;
    Rect viewport{0.0f, 0.0f, 200.0f, 60.0f};
    Point hit{30.0f, 15.0f};
    Point miss{180.0f, 50.0f};

    EXPECT_FALSE(tree.button->has_pseudo_state(Element::PseudoState::Hover));
    EXPECT_FALSE(tree.button->has_pseudo_state(Element::PseudoState::Active));

    EXPECT_TRUE(controller.set_control_interaction_at(tree.render_root.get(), hit, viewport, 0.0f));
    EXPECT_TRUE(tree.button->has_pseudo_state(Element::PseudoState::Hover));
    EXPECT_TRUE(tree.button->has_pseudo_state(Element::PseudoState::Active));

    EXPECT_TRUE(controller.set_control_interaction_at(tree.render_root.get(), miss, viewport, 0.0f));
    EXPECT_FALSE(tree.button->has_pseudo_state(Element::PseudoState::Hover));
    EXPECT_FALSE(tree.button->has_pseudo_state(Element::PseudoState::Active));
}
