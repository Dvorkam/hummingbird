#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <optional>

#include "app/BrowserApp.h"
#include "core/platform_api/IWindow.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
class QueuedWindow final : public Hummingbird::IWindow {
public:
    void open() override { is_open_ = true; }
    void update() override {}
    void close() override { is_open_ = false; }
    bool is_open() const override { return is_open_; }

    std::unique_ptr<Hummingbird::IGraphicsContext> get_graphics_context() override {
        return std::make_unique<Hummingbird::Test::TestGraphicsContext>();
    }

    std::pair<int, int> get_size() const override { return {1024, 768}; }

    bool wait_event(Hummingbird::InputEvent& out, int /*timeout_ms*/) override { return pop_event(out); }
    bool poll_event(Hummingbird::InputEvent& out) override { return pop_event(out); }

    void start_text_input() override {}
    void stop_text_input() override {}
    std::string get_clipboard_text() const override { return {}; }

    void push_event(Hummingbird::InputEvent event) { events_.push_back(std::move(event)); }

private:
    bool pop_event(Hummingbird::InputEvent& out) {
        if (events_.empty()) return false;
        out = std::move(events_.front());
        events_.pop_front();
        return true;
    }

    bool is_open_{false};
    std::deque<Hummingbird::InputEvent> events_;
};

Hummingbird::InputEvent make_ctrl_key(Hummingbird::Key key, bool shift = false) {
    Hummingbird::InputEvent event;
    event.type = Hummingbird::EventType::KeyDown;
    event.mods.ctrl = true;
    event.mods.shift = shift;
    event.key.key = key;
    return event;
}
}  // namespace

TEST(BrowserAppTabsTest, KeyboardShortcutsCreateSwitchAndCloseTabs) {
    auto window = std::make_unique<QueuedWindow>();
    ASSERT_NE(window, nullptr);
    window->open();
    ASSERT_TRUE(window->is_open());

    auto* window_ptr = window.get();
    Hummingbird::App::BrowserApp app(std::move(window));
    app.start();

    ASSERT_EQ(app.tab_count(), 1u);
    const auto first_id = app.active_tab_id();
    ASSERT_TRUE(first_id.has_value());

    window_ptr->push_event(make_ctrl_key(Hummingbird::Key::T));
    ASSERT_TRUE(app.tick());
    ASSERT_EQ(app.tab_count(), 2u);
    const auto second_id = app.active_tab_id();
    ASSERT_TRUE(second_id.has_value());
    EXPECT_NE(*second_id, *first_id);

    window_ptr->push_event(make_ctrl_key(Hummingbird::Key::Right));
    ASSERT_TRUE(app.tick());
    ASSERT_TRUE(app.active_tab_id().has_value());
    EXPECT_EQ(*app.active_tab_id(), *first_id);

    window_ptr->push_event(make_ctrl_key(Hummingbird::Key::Left));
    ASSERT_TRUE(app.tick());
    ASSERT_TRUE(app.active_tab_id().has_value());
    EXPECT_EQ(*app.active_tab_id(), *second_id);

    window_ptr->push_event(make_ctrl_key(Hummingbird::Key::W));
    ASSERT_TRUE(app.tick());
    EXPECT_EQ(app.tab_count(), 1u);
    ASSERT_TRUE(app.active_tab_id().has_value());
}
