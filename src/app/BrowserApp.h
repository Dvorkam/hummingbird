#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "engine/Tab.h"

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
    void render_if_needed();
    Hummingbird::Layout::Rect compute_content_viewport(int win_w, int win_h) const;
    void refresh_url_bar_render_text();

    // --- event handling ---
    void handle_event(const InputEvent& e);
    void handle_quit_event();
    void handle_text_input_event(const InputEvent& e);
    void handle_key_down_event(const InputEvent& e);
    void handle_mouse_down_event(const InputEvent& e);
    void handle_mouse_wheel_event(const InputEvent& e);
    void handle_resize_event(const InputEvent& e);
    void set_url_bar_active(bool active, const char* log_message);

private:
    // App Utils
    std::atomic<bool> shutting_down_{false};
    // Platform
    std::unique_ptr<IWindow> window_;
    std::unique_ptr<IGraphicsContext> graphics_;
    Hummingbird::Engine::Tab tab_;

    // UI state
    std::string url_bar_text_ = "https://example.dev";
    std::string url_bar_render_text_;
    std::string::size_type url_bar_caret_ = 0;
    std::string url_font_path_;
    TextStyle url_style_;
    bool url_bar_active_ = true;
    bool debug_outlines_ = false;
    bool needs_repaint_ = true;

    int url_bar_height_ = 32;

    // Event draining controls
    int max_events_per_tick_ = 200;
    int wait_timeout_ms_ = 16;
};
