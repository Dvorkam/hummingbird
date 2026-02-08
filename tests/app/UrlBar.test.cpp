#include "app/UrlBar.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"

namespace {

class TestWindow final : public Hummingbird::IWindow {
public:
    void open() override {}
    void update() override {}
    void close() override {}
    bool is_open() const override { return true; }

    std::unique_ptr<Hummingbird::IGraphicsContext> get_graphics_context() override { return nullptr; }
    std::pair<int, int> get_size() const override { return {0, 0}; }

    bool wait_event(Hummingbird::InputEvent& /*out*/, int /*timeout_ms*/) override { return false; }
    bool poll_event(Hummingbird::InputEvent& /*out*/) override { return false; }

    void start_text_input() override { ++start_text_calls; }
    void stop_text_input() override { ++stop_text_calls; }

    std::string get_clipboard_text() const override { return clipboard_text; }

    int start_text_calls = 0;
    int stop_text_calls = 0;
    std::string clipboard_text;
};

Hummingbird::InputEvent make_key_event(Hummingbird::Key key, bool repeat = false) {
    Hummingbird::InputEvent event;
    event.type = Hummingbird::EventType::KeyDown;
    event.key.key = key;
    event.key.repeat = repeat;
    return event;
}

Hummingbird::InputEvent make_paste_event(bool ctrl_v, bool shift_insert) {
    Hummingbird::InputEvent event;
    event.type = Hummingbird::EventType::KeyDown;
    if (ctrl_v) {
        event.mods.ctrl = true;
        event.key.key = Hummingbird::Key::V;
        return event;
    }
    event.mods.shift = shift_insert;
    event.key.key = Hummingbird::Key::Insert;
    return event;
}

}  // namespace

TEST(UrlBarTest, InsertsTextAndMovesCaret) {
    Hummingbird::App::UrlBar bar;
    bar.set_text("hello");

    auto left = make_key_event(Hummingbird::Key::Left);
    bar.handle_key_down(left, nullptr);
    bar.handle_text_input("X");

    EXPECT_EQ(bar.text(), "hellXo");
}

TEST(UrlBarTest, HomeEndKeysAffectInsertPosition) {
    Hummingbird::App::UrlBar bar;
    bar.set_text("abc");

    bar.handle_key_down(make_key_event(Hummingbird::Key::Home), nullptr);
    bar.handle_text_input("X");
    EXPECT_EQ(bar.text(), "Xabc");

    bar.handle_key_down(make_key_event(Hummingbird::Key::End), nullptr);
    bar.handle_text_input("Z");
    EXPECT_EQ(bar.text(), "XabcZ");
}

TEST(UrlBarTest, BackspaceAndDeleteEditText) {
    Hummingbird::App::UrlBar bar;
    bar.set_text("abc");

    bar.handle_key_down(make_key_event(Hummingbird::Key::Backspace), nullptr);
    EXPECT_EQ(bar.text(), "ab");

    bar.set_text("abc");
    bar.handle_key_down(make_key_event(Hummingbird::Key::Home), nullptr);
    bar.handle_key_down(make_key_event(Hummingbird::Key::Delete), nullptr);
    EXPECT_EQ(bar.text(), "bc");
}

TEST(UrlBarTest, EnterSubmitsAndEscapeCancels) {
    Hummingbird::App::UrlBar bar;
    TestWindow window;

    bar.set_text("https://acme.test");
    auto submit = bar.handle_key_down(make_key_event(Hummingbird::Key::Enter), &window);
    ASSERT_TRUE(submit.submitted_url.has_value());
    EXPECT_EQ(*submit.submitted_url, "https://acme.test");
    EXPECT_FALSE(bar.is_active());
    EXPECT_EQ(window.stop_text_calls, 1);

    bar.set_active(true, &window, nullptr);
    auto cancel = bar.handle_key_down(make_key_event(Hummingbird::Key::Escape), &window);
    EXPECT_FALSE(cancel.submitted_url.has_value());
    EXPECT_FALSE(bar.is_active());
    EXPECT_EQ(window.stop_text_calls, 2);
}

TEST(UrlBarTest, PasteUsesClipboardText) {
    Hummingbird::App::UrlBar bar;
    TestWindow window;
    window.clipboard_text = "paste";
    bar.set_text("abc");

    auto result = bar.handle_key_down(make_paste_event(true, false), &window);
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.needs_repaint);
    EXPECT_EQ(bar.text(), "abcpaste");
}

TEST(UrlBarTest, IgnoresTextInputWhenInactive) {
    Hummingbird::App::UrlBar bar;
    TestWindow window;
    bar.set_text("abc");
    bar.set_active(false, &window, nullptr);

    EXPECT_FALSE(bar.handle_text_input("z"));
    EXPECT_EQ(bar.text(), "abc");
}
