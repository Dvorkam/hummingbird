#include "platform/SDLWindow.h"
#include "platform/SDLGraphicsContext.h"
#include <SDL.h>
#include <iostream>

SDLWindow::SDLWindow() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
    }
}

SDLWindow::~SDLWindow() {
    close();
    SDL_Quit();
}

void SDLWindow::open() {
    m_window = SDL_CreateWindow("Hummingbird", 100, 100, 1024, 768, SDL_WINDOW_SHOWN);
    if (m_window == nullptr) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (m_renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        close();
        return;
    }

    m_is_open = true;
}

void SDLWindow::update() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_is_open = false;
        }
    }
}

void SDLWindow::close() {
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    m_is_open = false;
}

bool SDLWindow::is_open() const {
    return m_is_open;
}

std::unique_ptr<IGraphicsContext> SDLWindow::get_graphics_context() {
    return std::make_unique<SDLGraphicsContext>(m_renderer);
}