#include <gtest/gtest.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>

#include "app/BrowserApp.h"
#include "core/platform_api/IWindow.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
bool is_truthy_env(const char* value) {
    if (!value) return false;
    std::string_view view(value);
    auto equals = [&](std::string_view needle) {
        if (view.size() != needle.size()) return false;
        for (size_t i = 0; i < view.size(); ++i) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(view[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i])));
            if (a != b) return false;
        }
        return true;
    };
    return equals("1") || equals("true") || equals("yes") || equals("on");
}

class FakeWindow final : public Hummingbird::IWindow {
public:
    void open() override { is_open_ = true; }
    void update() override {}
    void close() override { is_open_ = false; }
    bool is_open() const override { return is_open_; }

    std::unique_ptr<Hummingbird::IGraphicsContext> get_graphics_context() override {
        return std::make_unique<Hummingbird::Test::TestGraphicsContext>();
    }

    std::pair<int, int> get_size() const override { return {1024, 768}; }

    bool wait_event(Hummingbird::InputEvent& /*out*/, int /*timeout_ms*/) override { return false; }
    bool poll_event(Hummingbird::InputEvent& /*out*/) override { return false; }
    void start_text_input() override {}
    void stop_text_input() override {}
    std::string get_clipboard_text() const override { return {}; }

private:
    bool is_open_{false};
};
}  // namespace

TEST(SmokeMainTest, StartsAndTicks) {
    if (!is_truthy_env(std::getenv("HB_RUN_SMOKE_TEST"))) {
        GTEST_SKIP() << "Set HB_RUN_SMOKE_TEST=1 to enable the smoke test.";
    }

    auto window = std::make_unique<FakeWindow>();
    ASSERT_NE(window, nullptr);
    window->open();
    ASSERT_TRUE(window->is_open());

    {
        auto gfx = window->get_graphics_context();
        ASSERT_NE(gfx, nullptr);
    }

    Hummingbird::App::BrowserApp app(std::move(window));
    app.start();

    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::seconds(5);

    int frames = 0;
    while (clock::now() < deadline) {
        if (!app.tick()) break;
        ++frames;
        if (frames >= 10) break;
    }

    app.shutdown();

    EXPECT_GT(frames, 0);
}
