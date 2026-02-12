#pragma once

#include <memory>
#include <optional>

#include "app/BrowserChrome.h"
#include "app/BrowserEventRouter.h"
#include "app/RenderCoordinator.h"
#include "app/TabController.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
class IWindow;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::Engine {
class ExtensionHost;
class FormSubmission;
class Tab;
}  // namespace Hummingbird::Engine

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
    Hummingbird::Layout::Rect compute_content_viewport(int win_w, int win_h) const;

    // --- event handling (routed by BrowserEventRouter) ---
    friend class BrowserEventRouter;
    friend class RenderCoordinator;
    void handle_quit_event();
    void handle_text_input_event(const InputEvent& e);
    void handle_key_down_event(const InputEvent& e);
    void handle_mouse_down_event(const InputEvent& e);
    void handle_mouse_wheel_event(const InputEvent& e);
    void handle_resize_event(const InputEvent& e);

private:
    void on_active_tab_changed();
    void sync_tab_text_input_mode();
    void tick_active_tab(const Hummingbird::Layout::Rect& viewport);
    void emit_navigation_commit_events();
    void sync_active_tab_security_state();
    void navigate_active_tab(std::string_view url);
    void navigate_active_tab(const Hummingbird::Engine::FormSubmission& submission);
    void initialize_extensions(Hummingbird::Engine::TabId first_tab_id);
    bool handle_tab_shortcut(const InputEvent& event);
    bool handle_url_bar_key_down(const InputEvent& event);
    bool handle_document_key_down(const InputEvent& event);
    bool handle_tab_strip_mouse_down(const InputEvent& event);
    bool handle_url_bar_mouse_down(const InputEvent& event);
    void handle_document_mouse_down(const InputEvent& event);
    void notify_extension_tab_created(Hummingbird::Engine::TabId tab_id, std::string_view url);
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
    TabController tab_controller_;
    std::unique_ptr<Hummingbird::Engine::ExtensionHost> extension_host_;
    BrowserEventRouter event_router_;

    // UI state
    BrowserChrome browser_chrome_;
    bool debug_outlines_ = false;
    bool tab_text_input_active_ = false;
    RenderCoordinator render_coordinator_;

    // Event draining controls
    int max_events_per_tick_ = 200;
    int wait_timeout_ms_ = 16;
};

}  // namespace Hummingbird::App
