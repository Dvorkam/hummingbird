#pragma once

#include <memory>
#include <optional>

#include "app/TabStrip.h"
#include "app/UrlBar.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "engine/extensions/ExtensionHost.h"
#include "engine/extensions/ExtensionLoader.h"
#include "engine/tab/Tab.h"
#include "engine/tab/TabManager.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
class IWindow;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::App {

class BrowserApp {
public:
    explicit BrowserApp(std::unique_ptr<IWindow> window);
    ~BrowserApp();
    void shutdown();

    // Starts the app: initial navigation, focus, etc.
    void start();

    // Runs one iteration: events + pipeline + render. Returns false when app should exit.
    bool tick();

    size_t tab_count() const;
    std::optional<Hummingbird::Engine::TabId> active_tab_id() const;

private:
    // --- main tick phases ---
    void pump_events();
    void render_if_needed();
    Hummingbird::Layout::Rect compute_content_viewport(int win_w, int win_h) const;

    // --- event handling ---
    void handle_event(const InputEvent& e);
    void handle_quit_event();
    void handle_text_input_event(const InputEvent& e);
    void handle_key_down_event(const InputEvent& e);
    void handle_mouse_down_event(const InputEvent& e);
    void handle_mouse_wheel_event(const InputEvent& e);
    void handle_resize_event(const InputEvent& e);

private:
    void on_active_tab_changed();
    void navigate_active_tab(std::string_view url);
    bool insert_extension_css(Hummingbird::Engine::TabId tab_id, std::string_view css_text);
    bool new_tab();
    bool close_active_tab();
    bool activate_next_tab();
    bool activate_prev_tab();

    Hummingbird::Engine::Tab& active_tab();

    // App Utils
    bool shutting_down_ = false;
    // Platform
    std::unique_ptr<IWindow> window_;
    std::unique_ptr<IGraphicsContext> graphics_;
    Hummingbird::Engine::TabManager tab_manager_;
    Hummingbird::Engine::ExtensionHost extension_host_;

    // UI state
    TabStrip tab_strip_;
    UrlBar url_bar_;
    bool debug_outlines_ = false;
    bool document_dirty_ = true;
    bool chrome_dirty_ = true;
    bool controls_dirty_ = false;
    bool document_cache_valid_ = false;

    // Event draining controls
    int max_events_per_tick_ = 200;
    int wait_timeout_ms_ = 16;
};

}  // namespace Hummingbird::App
