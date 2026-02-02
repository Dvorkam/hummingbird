#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "core/platform_api/ScriptEngineFactory.h"
#include "engine/tab/Tab.h"
#include "test_utils/TestGraphicsContext.h"

namespace Hummingbird::Test {

class HeadlessTabHarness {
public:
    HeadlessTabHarness(NetworkPtr network, NetworkPtr fallback, ResourceProviderPtr provider,
                       ImageDecoderPtr decoder = nullptr, ScriptEnginePtr script_engine = nullptr)
        : tab_(std::move(network), std::move(fallback), std::move(provider), std::move(decoder),
               script_engine ? std::move(script_engine) : Hummingbird::create_script_engine()) {}

    void set_viewport(const Layout::Rect& viewport) { viewport_ = viewport; }
    const Layout::Rect& viewport() const { return viewport_; }

    void navigate(std::string_view url) { tab_.navigate(url); }
    bool tick() { return tab_.tick(context_, viewport_); }
    void paint(bool debug_outlines = false) { tab_.paint(context_, viewport_, debug_outlines); }

    std::optional<Engine::ResourceView> resource_view(std::string_view url, Engine::ResourceType type) const {
        return tab_.resource_view(url, type);
    }

    Engine::Tab& tab() { return tab_; }
    const Engine::Tab& tab() const { return tab_; }

private:
    Hummingbird::Test::TestGraphicsContext context_;
    Layout::Rect viewport_{0, 0, 800, 600};
    Engine::Tab tab_;
};

}  // namespace Hummingbird::Test
