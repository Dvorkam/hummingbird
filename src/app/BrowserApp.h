#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "app/BrowserChrome.h"
#include "app/TabController.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
class IWindow;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::Engine {
class ExtensionHost;
struct FormSubmission;  // defined as a struct in engine/forms/FormSubmission.h
class Tab;
}  // namespace Hummingbird::Engine

namespace Hummingbird::App {

class BrowserEventRouter;
class ChromeEventRouter;
class DocumentEventRouter;
class RenderCoordinator;

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

    // --- orchestration API used by the event routers ---
    Hummingbird::Engine::Tab& active_tab();

    // Tab lifecycle actions; each keeps chrome, extensions, and text input in sync.
    bool new_tab();
    bool close_active_tab();
    bool activate_next_tab();
    bool activate_prev_tab();
    bool activate_tab(Hummingbird::Engine::TabId id);

    // Navigates the active tab and reflects the target in the URL bar + render state.
    void navigate_and_reflect_url(std::string_view url);
    void navigate_and_reflect_submission(const Hummingbird::Engine::FormSubmission& submission);

    // Whether platform text input is currently owned by the active tab (vs the URL bar).
    bool tab_text_input_active() const { return tab_text_input_active_; }
    void set_tab_text_input_active(bool active) { tab_text_input_active_ = active; }

private:
    // --- main tick phases ---
    void pump_events();
    void tick_active_tab(const Hummingbird::Layout::Rect& viewport);
    void emit_navigation_commit_events();
    void sync_active_tab_security_state();

    void on_active_tab_changed();
    void sync_tab_text_input_mode();
    void navigate_active_tab(std::string_view url);
    void navigate_active_tab(const Hummingbird::Engine::FormSubmission& submission);
    void initialize_extensions(Hummingbird::Engine::TabId first_tab_id);
    void notify_extension_tab_created(Hummingbird::Engine::TabId tab_id, std::string_view url);
    bool insert_extension_css(Hummingbird::Engine::TabId tab_id, std::string_view css_text);

    // App Utils
    bool shutting_down_ = false;
    // Platform
    std::unique_ptr<IWindow> window_;
    std::unique_ptr<IGraphicsContext> graphics_;
    TabController tab_controller_;
    std::unique_ptr<Hummingbird::Engine::ExtensionHost> extension_host_;

    // UI state
    BrowserChrome browser_chrome_;
    bool tab_text_input_active_ = false;

    // Rendering + event routing (routers depend on the render coordinator).
    std::unique_ptr<RenderCoordinator> render_coordinator_;
    std::unique_ptr<ChromeEventRouter> chrome_event_router_;
    std::unique_ptr<DocumentEventRouter> document_event_router_;
    std::unique_ptr<BrowserEventRouter> event_router_;

    // Event draining controls
    int max_events_per_tick_ = 200;
    int wait_timeout_ms_ = 16;
};

}  // namespace Hummingbird::App
