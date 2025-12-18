#pragma once

#include "core/IWindow.h"
#include <memory>

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

    std::unique_ptr<IGraphicsContext> get_graphics_context() override;

    SDL_Window* get_native_window() const { return m_window; }

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_is_open = false;
};
