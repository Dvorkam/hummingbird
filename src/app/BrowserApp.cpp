#include "app/BrowserApp.h"

#include <algorithm>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/ImageDecoderFactory.h"
#include "core/platform_api/InputEvent.h"
#include "core/platform_api/NetworkFactory.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/utils/Log.h"

namespace Hummingbird::App {

namespace {
// You can keep constants here to avoid re-allocating per frame
constexpr Color kClearColor{255, 255, 255, 255};
}  // namespace

BrowserApp::BrowserApp(std::unique_ptr<IWindow> window)
    : window_(std::move(window)),
      graphics_(window_ ? window_->get_graphics_context() : nullptr),
      tab_(create_network(NetworkBackend::Curl), create_network(NetworkBackend::Stub), create_resource_provider(),
           create_image_decoder()) {}

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
    if (url_bar_.handle_text_input(event.text.text)) {
        needs_repaint_ = true;
    }
}

void BrowserApp::handle_key_down_event(const InputEvent& event) {
    if (event.key.key == Key::L && event.mods.ctrl) {
        url_bar_.set_active(true, window_.get(), "[ui] URL bar focused");
        url_bar_.move_caret_to_end();
        needs_repaint_ = true;
        return;
    }

    auto result = url_bar_.handle_key_down(event, window_.get());
    if (result.handled) {
        if (result.submitted_url) {
            tab_.navigate(*result.submitted_url);
        }
        if (result.needs_repaint) {
            needs_repaint_ = true;
        }
        return;
    }

    if (event.key.key == Key::F1) {
        debug_outlines_ = !debug_outlines_;
        HB_LOG_INFO("[ui] Debug outlines " << (debug_outlines_ ? "ON" : "OFF"));
        needs_repaint_ = true;
        return;
    }

    needs_repaint_ = true;
}

void BrowserApp::handle_mouse_down_event(const InputEvent& event) {
    if (url_bar_.handle_mouse_down(event.mouse_button.x, event.mouse_button.y, window_.get())) {
        needs_repaint_ = true;
        return;
    }

    url_bar_.set_active(false, window_.get(), nullptr);

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
        url_bar_.set_text(*link);
        tab_.navigate(*link);
    }
    needs_repaint_ = true;
}

void BrowserApp::handle_mouse_wheel_event(const InputEvent& event) {
    const float delta = static_cast<float>(event.wheel.dy) * 32.0f;

    auto [win_w, win_h] = window_->get_size();
    const float viewport_h = static_cast<float>(std::max(0, win_h - url_bar_.height()));
    tab_.scroll_by(delta, viewport_h);

    needs_repaint_ = true;
}

void BrowserApp::handle_resize_event(const InputEvent& event) {
    needs_repaint_ = true;
}

Hummingbird::Layout::Rect BrowserApp::compute_content_viewport(int win_w, int win_h) const {
    const int content_h = std::max(0, win_h - url_bar_.height());
    return {0.0f, static_cast<float>(url_bar_.height()), static_cast<float>(win_w), static_cast<float>(content_h)};
}

void BrowserApp::render_if_needed() {
    if (!needs_repaint_ || !graphics_) return;

    auto [win_w, win_h] = window_->get_size();

    // Full viewport clear
    Hummingbird::Layout::Rect full{0, 0, static_cast<float>(win_w), static_cast<float>(win_h)};
    graphics_->set_viewport(full);
    graphics_->clear(kClearColor);

    // URL bar
    url_bar_.draw(*graphics_, win_w);

    // Document paint
    const auto viewport = compute_content_viewport(win_w, win_h);
    tab_.paint(*graphics_, viewport, debug_outlines_);

    graphics_->present();
    needs_repaint_ = false;
}

}  // namespace Hummingbird::App
