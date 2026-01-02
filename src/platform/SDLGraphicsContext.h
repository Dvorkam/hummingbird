#pragma once

#include <string>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"

// Forward declaration
struct SDL_Renderer;

class SDLGraphicsContext : public IGraphicsContext {
public:
    SDLGraphicsContext(SDL_Renderer* renderer);
    ~SDLGraphicsContext() override;

    void set_viewport(const Hummingbird::Layout::Rect& viewport) override;
    void clear(const Color& color) override;
    void present() override;
    void fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) override;
    TextMetrics measure_text(const std::string& text, const TextStyle& style) override;
    void draw_text(const std::string& text, float x, float y, const TextStyle& style) override;

private:
    SDL_Renderer* m_renderer = nullptr;
    Hummingbird::Layout::Rect m_viewport{0, 0, 0, 0};
};
