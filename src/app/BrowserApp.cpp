#include "app/BrowserApp.h"

#include <algorithm>
#include <utility>

#include "core/platform_api/NetworkFactory.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

namespace {
// You can keep constants here to avoid re-allocating per frame
constexpr Color kClearColor{255, 255, 255, 255};
constexpr Color kOverlayBg{220, 220, 220, 255};
constexpr Color kOverlayText{0, 0, 0, 255};
}  // namespace

BrowserApp::BrowserApp(std::unique_ptr<IWindow> window)
    : window_(std::move(window)),
      graphics_(window_ ? window_->get_graphics_context() : nullptr),
      tab_(create_network(NetworkBackend::Curl), create_network(NetworkBackend::Stub), create_resource_provider()) {
    url_bar_text_.reserve(2048);
    url_bar_render_text_.reserve(2049);

    url_font_path_ = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf").string();
    url_style_.font_path = url_font_path_;
    url_style_.font_size = 16.0f;
    url_style_.color = kOverlayText;

    refresh_url_bar_render_text();
}

BrowserApp::~BrowserApp() {
    shutdown();
}

void BrowserApp::shutdown() {
    // run once
    if (shutting_down_.exchange(true, std::memory_order_relaxed)) return;

    // stop input first (safe even if already stopped)
    if (window_) window_->stop_text_input();

    tab_.shutdown();

    // close window last (or earlier if you prefer to hide UI immediately)
    if (window_ && window_->is_open()) window_->close();
}

void BrowserApp::start() {
    // initial navigation
    tab_.navigate(url_bar_text_);

    // initial focus
    set_url_bar_active(true, "[ui] URL bar focused (default)");
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
            needs_repaint_ = true;
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
    if (!url_bar_active_) return;
    url_bar_text_ += event.text.text;
    refresh_url_bar_render_text();
    needs_repaint_ = true;
}

void BrowserApp::handle_key_down_event(const InputEvent& event) {
    if (event.key.key == Key::Backspace && url_bar_active_ && !url_bar_text_.empty()) {
        url_bar_text_.pop_back();
        refresh_url_bar_render_text();
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Enter) {
        set_url_bar_active(false, nullptr);
        tab_.navigate(url_bar_text_);
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Escape) {
        set_url_bar_active(false, nullptr);
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::F1) {
        debug_outlines_ = !debug_outlines_;
        HB_LOG_INFO("[ui] Debug outlines " << (debug_outlines_ ? "ON" : "OFF"));
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::L && event.mods.ctrl) {
        set_url_bar_active(true, "[ui] URL bar focused");
        needs_repaint_ = true;
        return;
    }

    needs_repaint_ = true;
}

void BrowserApp::handle_mouse_down_event(const InputEvent& event) {
    const int y = event.mouse_button.y;
    if (y < url_bar_height_) {
        set_url_bar_active(true, "[ui] URL bar focused (mouse)");
    } else {
        set_url_bar_active(false, nullptr);
    }
    needs_repaint_ = true;
}

void BrowserApp::handle_mouse_wheel_event(const InputEvent& event) {
    const float delta = static_cast<float>(event.wheel.dy) * 32.0f;

    auto [win_w, win_h] = window_->get_size();
    const float viewport_h = static_cast<float>(std::max(0, win_h - url_bar_height_));
    tab_.scroll_by(delta, viewport_h);

    needs_repaint_ = true;
}

void BrowserApp::handle_resize_event(const InputEvent& event) {
    needs_repaint_ = true;
}

void BrowserApp::set_url_bar_active(bool active, const char* log_message) {
    url_bar_active_ = active;
    if (window_) {
        if (active) {
            window_->start_text_input();
        } else {
            window_->stop_text_input();
        }
    }
    refresh_url_bar_render_text();
    if (log_message) {
        HB_LOG_INFO(log_message);
    }
}

Hummingbird::Layout::Rect BrowserApp::compute_content_viewport(int win_w, int win_h) const {
    const int content_h = std::max(0, win_h - url_bar_height_);
    return {0.0f, static_cast<float>(url_bar_height_), static_cast<float>(win_w), static_cast<float>(content_h)};
}

void BrowserApp::refresh_url_bar_render_text() {
    url_bar_render_text_.assign(url_bar_text_);
    if (url_bar_active_) {
        url_bar_render_text_.push_back('|');
    }
}

void BrowserApp::render_if_needed() {
    if (!needs_repaint_ || !graphics_) return;

    auto [win_w, win_h] = window_->get_size();

    // Full viewport clear
    Hummingbird::Layout::Rect full{0, 0, static_cast<float>(win_w), static_cast<float>(win_h)};
    graphics_->set_viewport(full);
    graphics_->clear(kClearColor);

    // URL bar
    Hummingbird::Layout::Rect bar{0, 0, static_cast<float>(win_w), static_cast<float>(url_bar_height_)};
    graphics_->fill_rect(bar, kOverlayBg);

    graphics_->draw_text(url_bar_render_text_, 8.0f, 8.0f, url_style_);

    // Document paint
    const auto viewport = compute_content_viewport(win_w, win_h);
    tab_.paint(*graphics_, viewport, debug_outlines_);

    graphics_->present();
    needs_repaint_ = false;
}
