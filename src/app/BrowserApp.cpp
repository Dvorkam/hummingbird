#include "app/BrowserApp.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "core/platform_api/ImageDecoderFactory.h"
#include "core/platform_api/NetworkFactory.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

namespace {
// You can keep constants here to avoid re-allocating per frame
constexpr Color kClearColor{255, 255, 255, 255};
constexpr Color kOverlayBg{220, 220, 220, 255};
constexpr Color kOverlayText{0, 0, 0, 255};

std::string::size_type clamp_caret(std::string::size_type caret, std::string_view text) {
    return std::min(caret, text.size());
}

std::string::size_type prev_codepoint(std::string_view text, std::string::size_type caret) {
    caret = clamp_caret(caret, text);
    if (caret == 0) return 0;
    std::string::size_type i = caret - 1;
    while (i > 0 && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
        --i;
    }
    return i;
}

std::string::size_type next_codepoint(std::string_view text, std::string::size_type caret) {
    caret = clamp_caret(caret, text);
    if (caret >= text.size()) return text.size();
    std::string::size_type i = caret + 1;
    while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
        ++i;
    }
    return i;
}
}  // namespace

BrowserApp::BrowserApp(std::unique_ptr<IWindow> window)
    : window_(std::move(window)),
      graphics_(window_ ? window_->get_graphics_context() : nullptr),
      tab_(create_network(NetworkBackend::Curl), create_network(NetworkBackend::Stub), create_resource_provider(),
           create_image_decoder()) {
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
    insert_url_bar_text(event.text.text);
}

void BrowserApp::handle_key_down_event(const InputEvent& event) {
    if (url_bar_active_ && !event.key.repeat) {
        const bool paste_ctrl_v = event.mods.ctrl && event.key.key == Key::V;
        const bool paste_shift_insert = event.mods.shift && event.key.key == Key::Insert;
        if (paste_ctrl_v || paste_shift_insert) {
            if (window_) {
                const auto clipboard_text = window_->get_clipboard_text();
                if (!clipboard_text.empty()) {
                    insert_url_bar_text(clipboard_text);
                }
            }
            return;
        }
    }

    if (event.key.key == Key::Backspace && url_bar_active_ && !url_bar_text_.empty()) {
        url_bar_caret_ = clamp_caret(url_bar_caret_, url_bar_text_);
        if (url_bar_caret_ > 0) {
            auto start = prev_codepoint(url_bar_text_, url_bar_caret_);
            url_bar_text_.erase(start, url_bar_caret_ - start);
            url_bar_caret_ = start;
        }
        refresh_url_bar_render_text();
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Delete && url_bar_active_ && !url_bar_text_.empty()) {
        url_bar_caret_ = clamp_caret(url_bar_caret_, url_bar_text_);
        if (url_bar_caret_ < url_bar_text_.size()) {
            auto end = next_codepoint(url_bar_text_, url_bar_caret_);
            url_bar_text_.erase(url_bar_caret_, end - url_bar_caret_);
        }
        refresh_url_bar_render_text();
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Left && url_bar_active_) {
        url_bar_caret_ = prev_codepoint(url_bar_text_, url_bar_caret_);
        refresh_url_bar_render_text();
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Right && url_bar_active_) {
        url_bar_caret_ = next_codepoint(url_bar_text_, url_bar_caret_);
        refresh_url_bar_render_text();
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Home && url_bar_active_) {
        url_bar_caret_ = 0;
        refresh_url_bar_render_text();
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::End && url_bar_active_) {
        url_bar_caret_ = url_bar_text_.size();
        refresh_url_bar_render_text();
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Enter && url_bar_active_) {
        set_url_bar_active(false, nullptr);
        tab_.navigate(url_bar_text_);
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Escape && url_bar_active_) {
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
        url_bar_caret_ = url_bar_text_.size();
        needs_repaint_ = true;
        return;
    }

    needs_repaint_ = true;
}

void BrowserApp::handle_mouse_down_event(const InputEvent& event) {
    const int y = event.mouse_button.y;
    if (y < url_bar_height_) {
        set_url_bar_active(true, "[ui] URL bar focused (mouse)");
        url_bar_caret_ = url_bar_text_.size();
        needs_repaint_ = true;
        return;
    }

    set_url_bar_active(false, nullptr);

    if (!graphics_ || !window_) {
        needs_repaint_ = true;
        return;
    }

    auto [win_w, win_h] = window_->get_size();
    const auto viewport = compute_content_viewport(win_w, win_h);
    Hummingbird::Layout::Point point{static_cast<float>(event.mouse_button.x),
                                     static_cast<float>(event.mouse_button.y)};
    auto link = tab_.hit_test_link(point, viewport);
    if (link) {
        url_bar_text_ = *link;
        url_bar_caret_ = url_bar_text_.size();
        refresh_url_bar_render_text();
        tab_.navigate(*link);
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
    if (url_bar_active_) {
        url_bar_caret_ = url_bar_text_.size();
    }
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
        url_bar_caret_ = clamp_caret(url_bar_caret_, url_bar_text_);
        url_bar_render_text_.insert(url_bar_caret_, "|");
    }
}

void BrowserApp::insert_url_bar_text(std::string_view text) {
    if (text.empty()) return;
    url_bar_caret_ = clamp_caret(url_bar_caret_, url_bar_text_);
    url_bar_text_.insert(url_bar_caret_, text);
    url_bar_caret_ += text.size();
    refresh_url_bar_render_text();
    needs_repaint_ = true;
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
