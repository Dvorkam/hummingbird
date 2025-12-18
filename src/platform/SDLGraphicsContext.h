#pragma once

#include "core/IGraphicsContext.h"

// Forward declaration
struct SDL_Renderer;

class SDLGraphicsContext : public IGraphicsContext {
public:
    SDLGraphicsContext(SDL_Renderer* renderer);
    ~SDLGraphicsContext() override;

    void clear(const Color& color) override;
    void present() override;

private:
    SDL_Renderer* m_renderer = nullptr;
};
