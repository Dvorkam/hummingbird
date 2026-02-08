#include "app/BrowserApp.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/ImageDecoderFactory.h"
#include "core/platform_api/InputEvent.h"
#include "core/platform_api/NetworkFactory.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "core/utils/Log.h"

namespace Hummingbird::App {

namespace {
// You can keep constants here to avoid re-allocating per frame
constexpr Color kClearColor{255, 255, 255, 255};

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

std::optional<ImageBitmap> load_icon(IResourceProvider* provider, IImageDecoder* decoder, std::string_view path) {
    if (!provider || !decoder) return std::nullopt;
    auto bytes = provider->load_bytes(path);
    if (!bytes) return std::nullopt;
    auto decoded = decoder->decode(*bytes);
    if (!decoded) {
        HB_LOG_WARN("[ui] failed to decode icon: " << path);
    }
    return decoded;
}

UrlBar::SecurityIcons load_security_icons(IResourceProvider* provider, IImageDecoder* decoder) {
    UrlBar::SecurityIcons icons;
    icons.secure = load_icon(provider, decoder, "assets/icons/page_security/secure.png");
    icons.insecure = load_icon(provider, decoder, "assets/icons/page_security/insecure.png");
    icons.asecure = load_icon(provider, decoder, "assets/icons/page_security/asecure.png");
    return icons;
}
}  // namespace

BrowserApp::BrowserApp(std::unique_ptr<IWindow> window)
    : window_(std::move(window)),
      graphics_(window_ ? window_->get_graphics_context() : nullptr),
      tab_(create_network(primary_backend_for_env()), create_network(NetworkBackend::Stub), create_resource_provider(),
           create_image_decoder(), create_script_engine()) {
    auto provider = create_resource_provider();
    auto decoder = create_image_decoder();
    if (provider && decoder) {
        url_bar_.set_security_icons(load_security_icons(provider.get(), decoder.get()));
    }
}

BrowserApp::~BrowserApp() {
    shutdown();
}

void BrowserApp::shutdown() {
    // run once
    if (shutting_down_) return;
    shutting_down_ = true;

    // stop input first (safe even if already stopped)
    if (window_) window_->stop_text_input();

    tab_.shutdown();

    // close window last (or earlier if you prefer to hide UI immediately)
    if (window_ && window_->is_open()) window_->close();
}

void BrowserApp::start() {
    // initial navigation
    tab_.navigate(url_bar_.text());

    // initial focus
    url_bar_.set_active(true, window_.get(), "[ui] URL bar focused (default)");
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
        if (tab_.tick(*graphics_, viewport)) {
            document_dirty_ = true;
        }
        if (url_bar_.set_security_state(tab_.security_state())) {
            chrome_dirty_ = true;
        }
    }
    render_if_needed();

    return window_->is_open();
}

void BrowserApp::pump_events() {
    InputEvent e;
    if (window_->wait_event(e, wait_timeout_ms_)) {
        handle_event(e);
    }

    int processed = 0;
    while (processed++ < max_events_per_tick_ && window_->poll_event(e)) {
        handle_event(e);
        if (!window_->is_open()) break;
    }
}

void BrowserApp::handle_event(const InputEvent& event) {
    switch (event.type) {
        case EventType::Quit:
            handle_quit_event();
            return;
        case EventType::TextInput:
            handle_text_input_event(event);
            return;
        case EventType::KeyDown:
            handle_key_down_event(event);
            return;
        case EventType::MouseDown:
            handle_mouse_down_event(event);
            return;
        case EventType::MouseWheel:
            handle_mouse_wheel_event(event);
            return;
        case EventType::Resize:
            handle_resize_event(event);
            return;
        default:
            return;
    }
}

void BrowserApp::handle_quit_event() {
    shutdown();
}

void BrowserApp::handle_text_input_event(const InputEvent& event) {
    if (url_bar_.handle_text_input(event.text.text)) {
        chrome_dirty_ = true;
        return;
    }
    if (tab_.handle_text_input(event.text.text)) {
        controls_dirty_ = true;
    }
}

void BrowserApp::handle_key_down_event(const InputEvent& event) {
    if (event.key.key == Key::L && event.mods.ctrl && event.mods.shift) {
        HB_LOG_INFO("[ui] Forcing document repaint");
        document_dirty_ = true;
        return;
    }

    if (event.key.key == Key::L && event.mods.ctrl) {
        url_bar_.set_active(true, window_.get(), "[ui] URL bar focused");
        url_bar_.move_caret_to_end();
        if (tab_.clear_input_focus()) {
            controls_dirty_ = true;
        }
        chrome_dirty_ = true;
        return;
    }

    auto result = url_bar_.handle_key_down(event, window_.get());
    if (result.handled) {
        if (result.submitted_url) {
            tab_.navigate(*result.submitted_url);
            document_dirty_ = true;
            chrome_dirty_ = true;
        }
        if (result.needs_repaint) {
            chrome_dirty_ = true;
        }
        return;
    }

    auto tab_result = tab_.handle_key_down(event);
    if (tab_result.handled) {
        if (tab_result.submitted_url) {
            url_bar_.set_text(*tab_result.submitted_url);
            tab_.navigate(*tab_result.submitted_url);
            document_dirty_ = true;
            chrome_dirty_ = true;
        }
        if (tab_result.needs_repaint) {
            controls_dirty_ = true;
        }
        return;
    }

    if (event.key.key == Key::F1) {
        debug_outlines_ = !debug_outlines_;
        HB_LOG_INFO("[ui] Debug outlines " << (debug_outlines_ ? "ON" : "OFF"));
        document_dirty_ = true;
        return;
    }
}

void BrowserApp::handle_mouse_down_event(const InputEvent& event) {
    auto url_result = url_bar_.handle_mouse_down(event.mouse_button.x, event.mouse_button.y, window_.get());
    if (url_result.handled) {
        if (url_result.security_override_requested) {
            if (tab_.allow_insecure_for_current_host()) {
                tab_.navigate(tab_.requested_url());
                document_dirty_ = true;
                chrome_dirty_ = true;
            }
        }
        if (tab_.clear_input_focus()) {
            controls_dirty_ = true;
        }
        if (url_result.needs_repaint) {
            chrome_dirty_ = true;
        }
        return;
    }

    url_bar_.set_active(false, window_.get(), nullptr);
    chrome_dirty_ = true;

    if (!graphics_ || !window_) {
        return;
    }

    auto [win_w, win_h] = window_->get_size();
    const auto viewport = compute_content_viewport(win_w, win_h);
    Hummingbird::Layout::Point point{static_cast<float>(event.mouse_button.x),
                                     static_cast<float>(event.mouse_button.y)};
    auto click_result = tab_.dispatch_click(point, viewport, *graphics_);
    if (click_result.mutated) {
        document_dirty_ = true;
    }
    bool was_focused = tab_.has_focused_input();
    bool now_focused = tab_.focus_input_at(point, viewport);
    if (was_focused != now_focused) {
        if (window_) {
            if (now_focused) {
                window_->start_text_input();
            } else {
                window_->stop_text_input();
            }
        }
        controls_dirty_ = true;
    }
    if (now_focused) {
        return;
    }
    auto submit = tab_.submit_form_at(point, viewport);
    if (submit) {
        url_bar_.set_text(*submit);
        tab_.navigate(*submit);
        document_dirty_ = true;
        chrome_dirty_ = true;
        return;
    }
    auto link = tab_.hit_test_link(point, viewport);
    if (link) {
        url_bar_.set_text(*link);
        tab_.navigate(*link);
        document_dirty_ = true;
        chrome_dirty_ = true;
    }
}

void BrowserApp::handle_mouse_wheel_event(const InputEvent& event) {
    const float delta = static_cast<float>(event.wheel.dy) * 32.0f;

    auto [win_w, win_h] = window_->get_size();
    const float viewport_h = static_cast<float>(std::max(0, win_h - url_bar_.height()));
    tab_.scroll_by(delta, viewport_h);

    document_dirty_ = true;
}

void BrowserApp::handle_resize_event(const InputEvent& event) {
    document_cache_valid_ = false;
    document_dirty_ = true;
}

Hummingbird::Layout::Rect BrowserApp::compute_content_viewport(int win_w, int win_h) const {
    const int content_h = std::max(0, win_h - url_bar_.height());
    return {0.0f, static_cast<float>(url_bar_.height()), static_cast<float>(win_w), static_cast<float>(content_h)};
}

void BrowserApp::render_if_needed() {
    if (!graphics_ || (!document_dirty_ && !chrome_dirty_ && !controls_dirty_)) return;

    auto [win_w, win_h] = window_->get_size();

    Hummingbird::Layout::Rect full{0, 0, static_cast<float>(win_w), static_cast<float>(win_h)};
    graphics_->set_viewport(full);
    const auto viewport = compute_content_viewport(win_w, win_h);
    if (!document_cache_valid_) {
        document_dirty_ = true;
    }
    if (document_dirty_) {
        // HB_LOG_DEBUG("[perf] render pass=document chrome=1 controls=1 scroll_y=" << tab_.scroll_y());
        document_cache_valid_ = false;
        if (graphics_->begin_document_cache(full)) {
            tab_.paint(*graphics_, viewport, debug_outlines_);
            graphics_->end_document_cache();
            document_cache_valid_ = true;
        } else {
            graphics_->clear(kClearColor);
            graphics_->set_text_cache_owner(0);
            url_bar_.draw(*graphics_, win_w);
            tab_.paint(*graphics_, viewport, debug_outlines_);
            graphics_->present();
            document_dirty_ = false;
            chrome_dirty_ = false;
            controls_dirty_ = false;
            return;
        }
    } else if (chrome_dirty_ && controls_dirty_) {
        // HB_LOG_DEBUG("[perf] render pass=chrome+controls scroll_y=" << tab_.scroll_y());
    } else if (chrome_dirty_) {
        // HB_LOG_DEBUG("[perf] render pass=chrome scroll_y=" << tab_.scroll_y());
    } else if (controls_dirty_) {
        // HB_LOG_DEBUG("[perf] render pass=controls scroll_y=" << tab_.scroll_y());
    }
    graphics_->clear(kClearColor);
    if (document_cache_valid_) {
        graphics_->draw_document_cache();
    }
    graphics_->set_viewport(full);
    graphics_->set_text_cache_owner(0);
    url_bar_.draw(*graphics_, win_w);
    if (!document_dirty_ && controls_dirty_) {
        tab_.paint_controls(*graphics_, viewport);
    }

    graphics_->present();
    document_dirty_ = false;
    chrome_dirty_ = false;
    controls_dirty_ = false;
}

}  // namespace Hummingbird::App
