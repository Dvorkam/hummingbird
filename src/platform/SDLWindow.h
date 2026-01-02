#pragma once

#include <memory>

#include "core/platform_api/IWindow.h"

// Forward declarations
struct SDL_Window;
struct SDL_Renderer;

class SDLWindow : public IWindow {
public:
    SDLWindow();
    ~SDLWindow() override;

    void open() override;
    void update() override;
    void close() override;
    bool is_open() const override;

    // Events raleted overrides
    bool wait_event(InputEvent& out, int timeout_ms) override;
    bool poll_event(InputEvent& out) override;
    void start_text_input() override;
    void stop_text_input() override;

    std::unique_ptr<IGraphicsContext> get_graphics_context() override;
    std::pair<int, int> get_size() const override;

    SDL_Window* get_native_window() const { return m_window; }

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_is_open = false;
};
