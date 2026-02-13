#include "app/BrowserApp.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "app/BrowserEventRouter.h"
#include "app/ExtensionBootstrap.h"
#include "app/RenderCoordinator.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/ImageDecoderFactory.h"
#include "core/platform_api/InputEvent.h"
#include "core/platform_api/NetworkFactory.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "core/utils/Log.h"
#include "engine/extensions/ExtensionHost.h"
#include "engine/tab/Tab.h"

namespace Hummingbird::App {

namespace {
bool env_truthy(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !value[0]) return false;
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

NetworkBackend primary_backend_for_env() {
    // Headless smoke should avoid libcurl lifecycle/teardown variance on CI.
    return env_truthy("HB_HEADLESS") ? NetworkBackend::Stub : NetworkBackend::Curl;
}

Hummingbird::Engine::TabFactory make_tab_factory(NetworkBackend primary_backend) {
    Hummingbird::Engine::TabFactory factory;
    factory.create_network = [primary_backend]() { return create_network(primary_backend); };
    factory.create_fallback_network = []() { return create_network(NetworkBackend::Stub); };
    factory.create_resource_provider = []() { return create_resource_provider(); };
    factory.create_image_decoder = []() { return create_image_decoder(); };
    factory.create_script_engine = []() { return create_script_engine(); };
    return factory;
}
}  // namespace

BrowserApp::BrowserApp(std::unique_ptr<IWindow> window)
    : window_(std::move(window)),
      graphics_(window_ ? window_->get_graphics_context() : nullptr),
      tab_controller_(make_tab_factory(primary_backend_for_env())),
      extension_host_(std::make_unique<Hummingbird::Engine::ExtensionHost>([]() { return create_script_engine(); })) {
    event_router_ = std::make_unique<BrowserEventRouter>(*this);
    render_coordinator_ = std::make_unique<RenderCoordinator>(*this);

    const auto first_tab_id = tab_controller_.create_tab();
    auto provider = create_resource_provider();
    auto decoder = create_image_decoder();
    if (provider && decoder) {
        browser_chrome_.load_security_icons(provider.get(), decoder.get());
    }

    initialize_extensions(first_tab_id);
}

void BrowserApp::initialize_extensions(Hummingbird::Engine::TabId first_tab_id) {
    auto bootstrap = Hummingbird::App::load_extension_bootstrap();
    for (const auto& e : bootstrap.errors) {
        HB_LOG_WARN("[ext] " << e.message << ": " << e.path.string());
    }
    extension_host_->set_settings(std::move(bootstrap.settings));
    extension_host_->set_insert_css_handler([this](Hummingbird::Engine::TabId tab_id, std::string_view css_text) {
        return insert_extension_css(tab_id, css_text);
    });
    extension_host_->set_extensions(std::move(bootstrap.extensions));
    if (extension_host_->extension_count() > 0) {
        HB_LOG_INFO("[ext] loaded extensions: " << extension_host_->extension_count());
    }
    extension_host_->start_background_scripts();
    notify_extension_tab_created(first_tab_id, browser_chrome_.url_bar().text());
    extension_host_->notify_tab_activated(first_tab_id);
}

BrowserApp::~BrowserApp() {
    shutdown();
}

size_t BrowserApp::tab_count() const {
    return tab_controller_.tab_count();
}

std::optional<Hummingbird::Engine::TabId> BrowserApp::active_tab_id() const {
    return tab_controller_.active_tab_id();
}

void BrowserApp::shutdown() {
    // run once
    if (shutting_down_) return;
    shutting_down_ = true;

    // stop input first (safe even if already stopped)
    if (window_) window_->stop_text_input();

    extension_host_->shutdown();
    tab_controller_.shutdown();

    // close window last (or earlier if you prefer to hide UI immediately)
    if (window_ && window_->is_open()) window_->close();
}

void BrowserApp::start() {
    // initial navigation
    navigate_active_tab(browser_chrome_.url_bar().text());

    // initial focus
    browser_chrome_.url_bar().set_active(true, window_.get(), "[ui] URL bar focused (default)");
    HB_LOG_INFO("[ui] Starting app. Press Ctrl+L to focus URL bar.");
}

bool BrowserApp::tick() {
    if (!window_ || !window_->is_open()) return false;

    window_->update();

    pump_events();

    if (!window_->is_open()) return false;

    if (graphics_) {
        auto [win_w, win_h] = window_->get_size();
        const auto viewport = compute_content_viewport(win_w, win_h);
        tick_active_tab(viewport);
        emit_navigation_commit_events();
        sync_active_tab_security_state();
        sync_tab_text_input_mode();
    }
    render_coordinator_->render_if_needed();

    return window_->is_open();
}

void BrowserApp::tick_active_tab(const Hummingbird::Layout::Rect& viewport) {
    if (active_tab().tick(*graphics_, viewport)) {
        render_coordinator_->set_document_dirty();
    }
}

void BrowserApp::emit_navigation_commit_events() {
    if (auto id = tab_controller_.active_tab_id()) {
        if (auto committed = active_tab().consume_navigation_commit_url()) {
            extension_host_->notify_tab_navigated(*id, *committed);
        }
    }
}

void BrowserApp::sync_active_tab_security_state() {
    if (browser_chrome_.url_bar().set_security_state(active_tab().security_state())) {
        render_coordinator_->set_chrome_dirty();
    }
}

void BrowserApp::pump_events() {
    InputEvent e;
    if (window_->wait_event(e, wait_timeout_ms_)) {
        event_router_->handle_event(e);
    }

    int processed = 0;
    while (processed++ < max_events_per_tick_ && window_->poll_event(e)) {
        event_router_->handle_event(e);
        if (!window_->is_open()) break;
    }
}

void BrowserApp::handle_quit_event() {
    shutdown();
}

void BrowserApp::handle_text_input_event(const InputEvent& event) {
    if (browser_chrome_.url_bar().handle_text_input(event.text.text)) {
        render_coordinator_->set_chrome_dirty();
        return;
    }
    if (active_tab().handle_text_input(event.text.text)) {
        render_coordinator_->set_controls_dirty();
    }
}

void BrowserApp::handle_key_down_event(const InputEvent& event) {
    if (handle_tab_shortcut(event)) {
        return;
    }

    if (event.key.key == Key::L && event.mods.ctrl && event.mods.shift) {
        HB_LOG_INFO("[ui] Forcing document repaint");
        render_coordinator_->set_document_dirty();
        return;
    }

    if (event.key.key == Key::L && event.mods.ctrl) {
        browser_chrome_.url_bar().set_active(true, window_.get(), "[ui] URL bar focused");
        browser_chrome_.url_bar().move_caret_to_end();
        if (active_tab().clear_input_focus()) {
            render_coordinator_->set_controls_dirty();
        }
        render_coordinator_->set_chrome_dirty();
        return;
    }

    if (handle_url_bar_key_down(event)) {
        return;
    }

    if (handle_document_key_down(event)) {
        return;
    }

    if (event.key.key == Key::F1) {
        debug_outlines_ = !debug_outlines_;
        HB_LOG_INFO("[ui] Debug outlines " << (debug_outlines_ ? "ON" : "OFF"));
        render_coordinator_->set_document_dirty();
        return;
    }
}

bool BrowserApp::handle_tab_shortcut(const InputEvent& event) {
    if (!event.key.repeat) {
        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::T) {
            if (new_tab()) {
                render_coordinator_->set_chrome_dirty();
                render_coordinator_->set_document_dirty();
                render_coordinator_->set_controls_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::W) {
            if (close_active_tab()) {
                render_coordinator_->set_chrome_dirty();
                render_coordinator_->set_document_dirty();
                render_coordinator_->set_controls_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::Right) {
            if (activate_next_tab()) {
                render_coordinator_->set_chrome_dirty();
                render_coordinator_->set_document_dirty();
                render_coordinator_->set_controls_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::Left) {
            if (activate_prev_tab()) {
                render_coordinator_->set_chrome_dirty();
                render_coordinator_->set_document_dirty();
                render_coordinator_->set_controls_dirty();
            }
            return true;
        }
    }
    return false;
}

bool BrowserApp::handle_url_bar_key_down(const InputEvent& event) {
    auto result = browser_chrome_.url_bar().handle_key_down(event, window_.get());
    if (!result.handled) {
        return false;
    }
    if (result.submitted_url) {
        navigate_active_tab(*result.submitted_url);
        render_coordinator_->set_document_dirty();
        render_coordinator_->set_chrome_dirty();
    }
    if (result.needs_repaint) {
        render_coordinator_->set_chrome_dirty();
    }
    return true;
}

bool BrowserApp::handle_document_key_down(const InputEvent& event) {
    auto tab_result = active_tab().handle_key_down(event);
    if (!tab_result.handled) {
        return false;
    }
    if (tab_result.submitted_form) {
        browser_chrome_.url_bar().set_text(tab_result.submitted_form->url);
        navigate_active_tab(*tab_result.submitted_form);
        render_coordinator_->set_document_dirty();
        render_coordinator_->set_chrome_dirty();
    }
    if (tab_result.needs_repaint) {
        render_coordinator_->set_controls_dirty();
    }
    return true;
}

void BrowserApp::handle_mouse_down_event(const InputEvent& event) {
    if (handle_tab_strip_mouse_down(event)) {
        return;
    }
    if (handle_url_bar_mouse_down(event)) {
        return;
    }
    handle_document_mouse_down(event);
}

bool BrowserApp::handle_tab_strip_mouse_down(const InputEvent& event) {
    if (!graphics_ || !window_) {
        return false;
    }
    const int tabs_top_y = browser_chrome_.url_bar().height();
    const int tabs_bottom_y = browser_chrome_.url_bar().height() + browser_chrome_.tab_strip_height();
    if (event.mouse_button.y < tabs_top_y || event.mouse_button.y >= tabs_bottom_y) {
        return false;
    }
    const auto [win_w, win_h] = window_->get_size();
    (void)win_h;
    auto result = browser_chrome_.handle_tab_strip_mouse_down(event.mouse_button.x, event.mouse_button.y, win_w,
                                                              tabs_top_y, tab_controller_.manager());
    if (!result.handled) {
        return false;
    }
    if (result.activated_tab && tab_controller_.set_active(*result.activated_tab)) {
        on_active_tab_changed();
    }
    render_coordinator_->set_chrome_dirty();
    render_coordinator_->set_document_dirty();
    render_coordinator_->set_controls_dirty();
    return true;
}

bool BrowserApp::handle_url_bar_mouse_down(const InputEvent& event) {
    auto url_result =
        browser_chrome_.url_bar().handle_mouse_down(event.mouse_button.x, event.mouse_button.y, window_.get());
    if (!url_result.handled) {
        return false;
    }

    bool interaction_state_changed = false;
    interaction_state_changed |= active_tab().clear_control_interaction();
    interaction_state_changed |= active_tab().clear_input_focus();
    if (interaction_state_changed && graphics_ && window_) {
        auto [w, h] = window_->get_size();
        const auto viewport = compute_content_viewport(w, h);
        if (active_tab().refresh_styles_for_interaction(*graphics_, viewport)) {
            render_coordinator_->set_document_dirty();
            render_coordinator_->set_controls_dirty();
        }
    } else if (interaction_state_changed) {
        render_coordinator_->set_document_dirty();
        render_coordinator_->set_controls_dirty();
    }
    if (url_result.security_override_requested) {
        if (active_tab().allow_insecure_for_current_host()) {
            navigate_active_tab(active_tab().requested_url());
            render_coordinator_->set_document_dirty();
            render_coordinator_->set_chrome_dirty();
        }
    }
    tab_text_input_active_ = false;
    if (url_result.needs_repaint) {
        render_coordinator_->set_chrome_dirty();
    }
    return true;
}

void BrowserApp::handle_document_mouse_down(const InputEvent& event) {
    browser_chrome_.url_bar().set_active(false, window_.get(), nullptr);
    render_coordinator_->set_chrome_dirty();

    if (!graphics_ || !window_) {
        return;
    }

    auto [win_w, win_h] = window_->get_size();
    const auto viewport = compute_content_viewport(win_w, win_h);
    Hummingbird::Layout::Point point{static_cast<float>(event.mouse_button.x),
                                     static_cast<float>(event.mouse_button.y)};
    HB_LOG_DEBUG("[input] mouse click at (" << point.x << "," << point.y << ") viewport=(" << viewport.x << ","
                                            << viewport.y << "," << viewport.width << "," << viewport.height << ")");
    auto click_result = active_tab().dispatch_click(point, viewport, *graphics_);
    if (click_result.mutated) {
        render_coordinator_->set_document_dirty();
    }
    bool interaction_changed = active_tab().set_control_interaction_at(point, viewport);
    bool was_focused = active_tab().has_focused_input();
    bool now_focused = active_tab().focus_input_at(point, viewport);
    HB_LOG_DEBUG("[input] focus probe was=" << was_focused << " now=" << now_focused);
    // URL bar deactivation stops platform text input. Re-enable it here whenever
    // a document input is focused, even if focus state did not transition.
    if (window_) {
        if (now_focused) {
            window_->start_text_input();
        } else {
            window_->stop_text_input();
        }
    }
    if (tab_text_input_active_ != now_focused || was_focused != now_focused) {
        tab_text_input_active_ = now_focused;
        render_coordinator_->set_controls_dirty();
    }
    if (interaction_changed || was_focused || now_focused) {
        if (active_tab().refresh_styles_for_interaction(*graphics_, viewport)) {
            render_coordinator_->set_document_dirty();
            render_coordinator_->set_controls_dirty();
        }
    }
    if (now_focused) {
        return;
    }
    auto submit = active_tab().submit_form_at(point, viewport);
    if (submit) {
        HB_LOG_DEBUG("[input] submit hit method="
                     << (submit->method == Hummingbird::Engine::FormSubmitMethod::Post ? "POST" : "GET")
                     << " url=" << submit->url);
        browser_chrome_.url_bar().set_text(submit->url);
        navigate_active_tab(*submit);
        render_coordinator_->set_document_dirty();
        render_coordinator_->set_chrome_dirty();
        return;
    }
    auto link = active_tab().hit_test_link(point, viewport);
    if (link) {
        browser_chrome_.url_bar().set_text(*link);
        navigate_active_tab(*link);
        render_coordinator_->set_document_dirty();
        render_coordinator_->set_chrome_dirty();
    }
}

void BrowserApp::handle_mouse_wheel_event(const InputEvent& event) {
    const float delta = static_cast<float>(event.wheel.dy) * 32.0f;

    auto [win_w, win_h] = window_->get_size();
    const float viewport_h = static_cast<float>(
        std::max(0, win_h - browser_chrome_.url_bar().height() - browser_chrome_.tab_strip_height()));
    active_tab().scroll_by(delta, viewport_h);

    render_coordinator_->set_document_dirty();
}

void BrowserApp::handle_resize_event(const InputEvent& event) {
    render_coordinator_->invalidate_document_cache();
}

Hummingbird::Layout::Rect BrowserApp::compute_content_viewport(int win_w, int win_h) const {
    const int content_y = browser_chrome_.url_bar().height() + browser_chrome_.tab_strip_height();
    const int content_h = std::max(0, win_h - content_y);
    return {0.0f, static_cast<float>(content_y), static_cast<float>(win_w), static_cast<float>(content_h)};
}

Hummingbird::Engine::Tab& BrowserApp::active_tab() {
    auto* tab = tab_controller_.active_tab();
    // BrowserApp expects an active tab for all operations; TabManager is responsible
    // for maintaining that invariant for the app.
    if (!tab) {
        tab_controller_.create_tab();
        tab = tab_controller_.active_tab();
    }
    return *tab;
}

void BrowserApp::on_active_tab_changed() {
    if (!window_) return;

    const auto* tab = tab_controller_.active_tab();
    if (!tab) return;

    browser_chrome_.url_bar().set_text(tab->requested_url());
    browser_chrome_.url_bar().set_security_state(tab->security_state());
    render_coordinator_->invalidate_document_cache();
    render_coordinator_->set_chrome_dirty();
    render_coordinator_->set_controls_dirty();

    window_->stop_text_input();
    tab_text_input_active_ = false;
    sync_tab_text_input_mode();

    if (auto id = tab_controller_.active_tab_id()) {
        extension_host_->notify_tab_activated(*id);
    }
}

void BrowserApp::sync_tab_text_input_mode() {
    if (!window_) return;
    if (browser_chrome_.url_bar().is_active()) {
        tab_text_input_active_ = false;
        return;
    }

    const bool should_be_active = active_tab().has_focused_input();
    if (should_be_active == tab_text_input_active_) {
        return;
    }

    if (should_be_active) {
        window_->start_text_input();
    } else {
        window_->stop_text_input();
    }
    tab_text_input_active_ = should_be_active;
}

void BrowserApp::navigate_active_tab(std::string_view url) {
    active_tab().navigate(url);
}

void BrowserApp::navigate_active_tab(const Hummingbird::Engine::FormSubmission& submission) {
    active_tab().navigate(submission);
}

void BrowserApp::notify_extension_tab_created(Hummingbird::Engine::TabId tab_id, std::string_view url) {
    extension_host_->notify_tab_created(tab_id, url);
}

bool BrowserApp::insert_extension_css(Hummingbird::Engine::TabId tab_id, std::string_view css_text) {
    auto* tab = tab_controller_.tab_by_id(tab_id);
    if (!tab) {
        return false;
    }
    if (!tab->insert_extension_css(css_text)) {
        return false;
    }
    render_coordinator_->set_document_dirty();
    render_coordinator_->set_controls_dirty();
    return true;
}

bool BrowserApp::new_tab() {
    const auto id = tab_controller_.create_tab();
    // Keep this simple for MVP: new tabs start on the current URL bar target.
    navigate_active_tab(browser_chrome_.url_bar().text());
    on_active_tab_changed();
    notify_extension_tab_created(id, browser_chrome_.url_bar().text());
    return true;
}

bool BrowserApp::close_active_tab() {
    if (!tab_controller_.close_active()) return false;
    if (!tab_controller_.active_tab()) {
        tab_controller_.create_tab();
    }
    on_active_tab_changed();
    return true;
}

bool BrowserApp::activate_next_tab() {
    if (!tab_controller_.activate_next()) return false;
    on_active_tab_changed();
    return true;
}

bool BrowserApp::activate_prev_tab() {
    if (!tab_controller_.activate_prev()) return false;
    on_active_tab_changed();
    return true;
}
}  // namespace Hummingbird::App
