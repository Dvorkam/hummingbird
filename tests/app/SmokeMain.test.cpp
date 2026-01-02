#include <gtest/gtest.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <utility>

#include "app/BrowserApp.h"
#include "core/platform_api/WindowFactory.h"

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
}  // namespace

TEST(SmokeMainTest, StartsAndTicks) {
    if (!is_truthy_env(std::getenv("HB_RUN_SMOKE_TEST"))) {
        GTEST_SKIP() << "Set HB_RUN_SMOKE_TEST=1 to enable the smoke test.";
    }

    auto window = create_window();
    ASSERT_NE(window, nullptr);
    window->open();
    ASSERT_TRUE(window->is_open());

    auto gfx = window->get_graphics_context();
    ASSERT_NE(gfx, nullptr);

    BrowserApp app(std::move(window));
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
