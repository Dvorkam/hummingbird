#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "core/ArenaAllocator.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/INetwork.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "layout/TreeBuilder.h"
#include "renderer/Painter.h"
#include "style/StyleEngine.h"

// Forward decls (or include appropriate DOM/Layout headers if needed)
namespace Hummingbird::DOM {
class Node;
}
namespace Hummingbird::Layout {
class RenderObject;
struct Rect;
}  // namespace Hummingbird::Layout

class BrowserApp {
public:
    explicit BrowserApp(std::unique_ptr<IWindow> window);
    ~BrowserApp();
    void shutdown();

    // Starts the app: initial navigation, focus, etc.
    void start();

    // Runs one iteration: events + pipeline + render. Returns false when app should exit.
    bool tick();

private:
    // --- main tick phases ---
    void pump_events();
    void consume_pending_html_and_rebuild();
    void render_if_needed();

    // --- event handling ---
    void handle_event(const InputEvent& e);

    // --- navigation ---
    void load_url(const std::string& url);

    // --- helpers ---
    void clamp_scroll(float viewport_height);
    void relayout_for_window(int win_w, int win_h);

private:
    // App Utils
    std::atomic<bool> shutting_down_{false};
    // Platform
    std::unique_ptr<IWindow> window_;
    std::unique_ptr<IGraphicsContext> graphics_;

    // Async HTML handoff (network thread -> main thread)
    std::mutex pending_mutex_;
    std::optional<std::string> pending_html_;

    // Deps / subsystems
    std::unique_ptr<INetwork> network_;
    std::unique_ptr<INetwork> fallback_network_;
    Hummingbird::Css::StyleEngine style_engine_;
    Hummingbird::Layout::TreeBuilder tree_builder_;
    Hummingbird::Renderer::Painter painter_;

    // UI state
    std::string url_bar_text_ = "http://bettermotherfuckingwebsite.com/";
    std::string requested_url_ = url_bar_text_;
    bool url_bar_active_ = true;
    bool debug_outlines_ = false;
    bool needs_repaint_ = true;

    int url_bar_height_ = 32;
    float scroll_y_ = 0.0f;
    float content_height_ = 0.0f;

    // Navigation race protection
    std::atomic<uint64_t> nav_counter_{0};
    std::atomic<uint64_t> active_nav_{0};

    // Document / layout state
    ArenaAllocator dom_arena_{2 * 1024 * 1024};
    ArenaPtr<Hummingbird::DOM::Node> dom_tree_;
    std::unique_ptr<Hummingbird::Layout::RenderObject> render_tree_;

    // Event draining controls
    int max_events_per_tick_ = 200;
    int wait_timeout_ms_ = 16;
};
