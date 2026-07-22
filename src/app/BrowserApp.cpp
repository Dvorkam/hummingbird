#include "app/BrowserApp.h"

#include <cctype>
#include <cstdlib>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

#include "app/BrowserEventRouter.h"
#include "app/ChromeEventRouter.h"
#include "app/DocumentEventRouter.h"
#include "app/ExtensionBootstrap.h"
#include "app/RenderCoordinator.h"
#include "core/GraphicsTypes.h"
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
    render_coordinator_ =
        std::make_unique<RenderCoordinator>(window_.get(), graphics_.get(), browser_chrome_, tab_controller_);
    chrome_event_router_ = std::make_unique<ChromeEventRouter>(*this, browser_chrome_, tab_controller_,
                                                               *render_coordinator_, window_.get(), graphics_.get());
    document_event_router_ = std::make_unique<DocumentEventRouter>(*this, browser_chrome_, *render_coordinator_,
                                                                   window_.get(), graphics_.get());
    event_router_ = std::make_unique<BrowserEventRouter>(*this, *chrome_event_router_, *document_event_router_,
                                                         *render_coordinator_);

    // Story 8.1.4: restore the profile's persistent cookies before the first
    // tab exists, so the very first navigation is already authenticated. Kept at
    // the app layer rather than in TabManager so engine tests never touch disk.
    if (const auto& jar = tab_controller_.manager().cookie_jar()) {
        const size_t restored = jar->load_from(Hummingbird::Core::CookieJar::default_path(),
                                               Hummingbird::Core::CookieClock::now());
        if (restored > 0) {
            HB_LOG_INFO("[cookies] restored " << restored << " cookie(s)");
        }
    }

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

    // Persist before anything is torn down. Session cookies are dropped by
    // save_to itself, so closing the browser really does end the session.
    if (const auto& jar = tab_controller_.manager().cookie_jar()) {
        const size_t saved = jar->save_to(Hummingbird::Core::CookieJar::default_path(),
                                          Hummingbird::Core::CookieClock::now());
        HB_LOG_DEBUG("[cookies] persisted " << saved << " cookie(s)");
    }

    // localStorage (8.2.2). No startup restore is needed — the manager loads each
    // origin lazily on first access — so only the shutdown flush lives here.
    if (const auto& storage = tab_controller_.manager().storage_manager()) {
        const size_t saved = storage->save_all();
        HB_LOG_DEBUG("[storage] persisted " << saved << " origin store(s)");
    }

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
        const auto viewport = browser_chrome_.content_viewport(win_w, win_h);
        tick_active_tab(viewport);
        emit_navigation_commit_events();
        sync_active_tab_security_state();
        sync_active_tab_url();
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

void BrowserApp::sync_active_tab_url() {
    if (auto url = active_tab().consume_url_bar_update()) {
        browser_chrome_.url_bar().set_text(*url);
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

Hummingbird::Engine::Tab& BrowserApp::active_tab() {
    return tab_controller_.ensure_active_tab();
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

void BrowserApp::navigate_active_tab(std::string_view url, Hummingbird::Engine::NavigationSource source) {
    active_tab().navigate(url, source);
}

void BrowserApp::navigate_active_tab(const Hummingbird::Engine::FormSubmission& submission) {
    active_tab().navigate(submission);
}

void BrowserApp::navigate_and_reflect_url(std::string_view url, Hummingbird::Engine::NavigationSource source) {
    browser_chrome_.url_bar().set_text(url);
    navigate_active_tab(url, source);
    render_coordinator_->set_document_dirty();
    render_coordinator_->set_chrome_dirty();
}

void BrowserApp::navigate_and_reflect_submission(const Hummingbird::Engine::FormSubmission& submission) {
    browser_chrome_.url_bar().set_text(submission.url);
    navigate_active_tab(submission);
    render_coordinator_->set_document_dirty();
    render_coordinator_->set_chrome_dirty();
}

void BrowserApp::navigate_back() {
    if (!window_ || !graphics_) return;
    auto [win_w, win_h] = window_->get_size();
    const auto viewport = browser_chrome_.content_viewport(win_w, win_h);
    if (active_tab().go_back(*graphics_, viewport)) {
        browser_chrome_.url_bar().set_text(active_tab().requested_url());
        render_coordinator_->set_document_dirty();
        render_coordinator_->set_chrome_dirty();
    }
}

void BrowserApp::navigate_forward() {
    if (!window_ || !graphics_) return;
    auto [win_w, win_h] = window_->get_size();
    const auto viewport = browser_chrome_.content_viewport(win_w, win_h);
    if (active_tab().go_forward(*graphics_, viewport)) {
        browser_chrome_.url_bar().set_text(active_tab().requested_url());
        render_coordinator_->set_document_dirty();
        render_coordinator_->set_chrome_dirty();
    }
}

void BrowserApp::bookmark_active_tab() {
    std::string url(active_tab().requested_url());
    if (url.empty() || url == "about:bookmarks") {
        return;  // nothing meaningful to bookmark (incl. the bookmarks page itself)
    }
    // Title extraction is a follow-up; use the URL as the label for now (MVP).
    if (bookmarks_.add(url, url)) {
        bookmarks_.save();
        HB_LOG_INFO("[bookmarks] added " << url);
    }
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
    render_coordinator_->set_document_and_controls_dirty();
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

bool BrowserApp::activate_tab(Hummingbird::Engine::TabId id) {
    if (!tab_controller_.set_active(id)) return false;
    on_active_tab_changed();
    return true;
}
}  // namespace Hummingbird::App
