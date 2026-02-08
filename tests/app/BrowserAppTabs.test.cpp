#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <optional>

#include "app/BrowserApp.h"
#include "core/platform_api/IWindow.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
class CountingGraphicsContext final : public Hummingbird::IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& /*viewport*/) override {}
    void clear(const Hummingbird::Color& /*color*/) override { ++clear_calls; }
    void present() override { ++present_calls; }
    void fill_rect(const Hummingbird::Layout::Rect& /*rect*/, const Hummingbird::Color& /*color*/) override {}
    void draw_image(const Hummingbird::ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override {}
    Hummingbird::TextMetrics measure_text(const std::string& text, const Hummingbird::TextStyle& style) override {
        Hummingbird::TextMetrics metrics;
        const float font_size = style.font_size > 0.0f ? style.font_size : 16.0f;
        metrics.width = static_cast<float>(text.size()) * font_size * 0.5f;
        metrics.height = font_size;
        metrics.ascent = font_size * 0.8f;
        metrics.descent = font_size * 0.2f;
        return metrics;
    }
    void draw_text(const std::string& /*text*/, float /*x*/, float /*y*/,
                   const Hummingbird::TextStyle& /*style*/) override {}

    bool begin_document_cache(const Hummingbird::Layout::Rect& /*viewport*/) override {
        ++begin_document_cache_calls;
        return true;
    }
    void end_document_cache() override { ++end_document_cache_calls; }
    void draw_document_cache() override { ++draw_document_cache_calls; }

    int clear_calls = 0;
    int present_calls = 0;
    int begin_document_cache_calls = 0;
    int end_document_cache_calls = 0;
    int draw_document_cache_calls = 0;
};

class QueuedWindow final : public Hummingbird::IWindow {
public:
    void open() override { is_open_ = true; }
    void update() override {}
    void close() override { is_open_ = false; }
    bool is_open() const override { return is_open_; }

    std::unique_ptr<Hummingbird::IGraphicsContext> get_graphics_context() override {
        auto ctx = std::make_unique<CountingGraphicsContext>();
        graphics_context_ = ctx.get();
        return ctx;
    }

    std::pair<int, int> get_size() const override { return {1024, 768}; }

    bool wait_event(Hummingbird::InputEvent& out, int /*timeout_ms*/) override { return pop_event(out); }
    bool poll_event(Hummingbird::InputEvent& out) override { return pop_event(out); }

    void start_text_input() override {}
    void stop_text_input() override {}
    std::string get_clipboard_text() const override { return {}; }

    void push_event(Hummingbird::InputEvent event) { events_.push_back(std::move(event)); }
    CountingGraphicsContext* graphics_context() const { return graphics_context_; }

private:
    bool pop_event(Hummingbird::InputEvent& out) {
        if (events_.empty()) return false;
        out = std::move(events_.front());
        events_.pop_front();
        return true;
    }

    bool is_open_{false};
    std::deque<Hummingbird::InputEvent> events_;
    CountingGraphicsContext* graphics_context_ = nullptr;
};

Hummingbird::InputEvent make_ctrl_key(Hummingbird::Key key, bool shift = false) {
    Hummingbird::InputEvent event;
    event.type = Hummingbird::EventType::KeyDown;
    event.mods.ctrl = true;
    event.mods.shift = shift;
    event.key.key = key;
    return event;
}

Hummingbird::InputEvent make_text_input(std::string text) {
    Hummingbird::InputEvent event;
    event.type = Hummingbird::EventType::TextInput;
    event.text.text = std::move(text);
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

TEST(BrowserAppTabsTest, UrlBarTypingReusesDocumentCacheAfterWarmup) {
    auto window = std::make_unique<QueuedWindow>();
    ASSERT_NE(window, nullptr);
    window->open();
    auto* window_ptr = window.get();

    Hummingbird::App::BrowserApp app(std::move(window));
    app.start();
    auto* graphics = window_ptr->graphics_context();
    ASSERT_NE(graphics, nullptr);

    int stable_ticks = 0;
    int last_begin_count = graphics->begin_document_cache_calls;
    for (int i = 0; i < 20 && stable_ticks < 3; ++i) {
        ASSERT_TRUE(app.tick());
        if (graphics->begin_document_cache_calls == last_begin_count) {
            ++stable_ticks;
        } else {
            stable_ticks = 0;
            last_begin_count = graphics->begin_document_cache_calls;
        }
    }

    const int begin_before_input = graphics->begin_document_cache_calls;
    const int draw_before_input = graphics->draw_document_cache_calls;
    window_ptr->push_event(make_text_input("x"));
    ASSERT_TRUE(app.tick());

    EXPECT_EQ(graphics->begin_document_cache_calls, begin_before_input);
    EXPECT_GT(graphics->draw_document_cache_calls, draw_before_input);
}
