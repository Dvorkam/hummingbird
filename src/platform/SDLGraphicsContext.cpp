#include "platform/SDLGraphicsContext.h"
#include <SDL.h>

SDLGraphicsContext::SDLGraphicsContext(SDL_Renderer* renderer) : m_renderer(renderer) {
}

SDLGraphicsContext::~SDLGraphicsContext() {
    // The context does not own the renderer, so it does not destroy it.
}

void SDLGraphicsContext::clear(const Color& color) {
    if (m_renderer) {
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_RenderClear(m_renderer);
    }
}

void SDLGraphicsContext::present() {
    if (m_renderer) {
        SDL_RenderPresent(m_renderer);
    }
}
