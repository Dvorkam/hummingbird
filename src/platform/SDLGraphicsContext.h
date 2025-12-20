#pragma once

#include <string>

#include "core/IGraphicsContext.h"
#include "layout/RenderObject.h"

// Forward declaration
struct SDL_Renderer;

class SDLGraphicsContext : public IGraphicsContext {
public:
    SDLGraphicsContext(SDL_Renderer* renderer);
    ~SDLGraphicsContext() override;

    void clear(const Color& color) override;
    void present() override;
    void fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) override;
    TextMetrics measure_text(const std::string& text, const std::string& font_path, float font_size, bool bold,
                             bool italic, bool monospace) override;
    void draw_text(const std::string& text, float x, float y, const std::string& font_path, float font_size,
                   const Color& color, bool bold, bool italic, bool monospace) override;

private:
    SDL_Renderer* m_renderer = nullptr;
};
