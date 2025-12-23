#include "platform/SDLWindow.h"

#include <SDL.h>

#include <iostream>

#include "platform/SDLGraphicsContext.h"

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
    // Event polling is handled in the app main loop.
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

std::pair<int, int> SDLWindow::get_size() const {
    int w = 0, h = 0;
    if (m_window) {
        SDL_GetWindowSize(m_window, &w, &h);
    }
    return {w, h};
}

static Modifiers to_mods(SDL_Keymod mod) {
    Modifiers m;
    m.ctrl = (mod & KMOD_CTRL) != 0;
    m.shift = (mod & KMOD_SHIFT) != 0;
    m.alt = (mod & KMOD_ALT) != 0;
    m.meta = (mod & KMOD_GUI) != 0;
    return m;
}

static Key to_key(SDL_Keycode kc) {
    if (kc >= SDLK_a && kc <= SDLK_z) {
        return static_cast<Key>(static_cast<uint8_t>(Key::A) + static_cast<uint8_t>(kc - SDLK_a));
    }
    switch (kc) {
        case SDLK_BACKSPACE:
            return Key::Backspace;
        case SDLK_RETURN:
            return Key::Enter;
        case SDLK_ESCAPE:
            return Key::Escape;
        case SDLK_F1:
            return Key::F1;
        default:
            return Key::Unknown;
    }
}

static MouseButton to_mouse_button(uint8_t b) {
    switch (b) {
        case SDL_BUTTON_LEFT:
            return MouseButton::Left;
        case SDL_BUTTON_MIDDLE:
            return MouseButton::Middle;
        case SDL_BUTTON_RIGHT:
            return MouseButton::Right;
        case SDL_BUTTON_X1:
            return MouseButton::X1;
        case SDL_BUTTON_X2:
            return MouseButton::X2;
        default:
            return MouseButton::Unknown;
    }
}

static bool translate_event(const SDL_Event& e, InputEvent& out) {
    out = {};  // reset

    switch (e.type) {
        case SDL_QUIT:
            out.type = EventType::Quit;
            return true;

        case SDL_TEXTINPUT:
            out.type = EventType::TextInput;
            out.text.text = e.text.text;  // UTF-8
            return true;

        case SDL_KEYDOWN:
            out.type = EventType::KeyDown;
            out.mods = to_mods(static_cast<SDL_Keymod>(e.key.keysym.mod));
            out.key.key = to_key(e.key.keysym.sym);
            out.key.repeat = (e.key.repeat != 0);
            return true;

        case SDL_KEYUP:
            out.type = EventType::KeyUp;
            out.mods = to_mods(static_cast<SDL_Keymod>(e.key.keysym.mod));
            out.key.key = to_key(e.key.keysym.sym);
            out.key.repeat = false;
            return true;

        case SDL_MOUSEBUTTONDOWN:
            out.type = EventType::MouseDown;
            out.mouse_button.x = e.button.x;
            out.mouse_button.y = e.button.y;
            out.mouse_button.button = to_mouse_button(e.button.button);
            return true;

        case SDL_MOUSEBUTTONUP:
            out.type = EventType::MouseUp;
            out.mouse_button.x = e.button.x;
            out.mouse_button.y = e.button.y;
            out.mouse_button.button = to_mouse_button(e.button.button);
            return true;

        case SDL_MOUSEWHEEL: {
            out.type = EventType::MouseWheel;
            float dx = static_cast<float>(e.wheel.x);
            float dy = static_cast<float>(e.wheel.y);
            if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                dx = -dx;
                dy = -dy;
            }
            out.wheel.dx = dx;
            out.wheel.dy = dy;
            return true;
        }

        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED) {
                out.type = EventType::Resize;
                out.resize.width = e.window.data1;
                out.resize.height = e.window.data2;
                return true;
            }
            return false;

        default:
            return false;
    }
}

bool SDLWindow::wait_event(InputEvent& out, int timeout_ms) {
    SDL_Event e;
    if (!SDL_WaitEventTimeout(&e, timeout_ms)) return false;
    return translate_event(e, out);
}

bool SDLWindow::poll_event(InputEvent& out) {
    SDL_Event e;
    if (!SDL_PollEvent(&e)) return false;
    return translate_event(e, out);
}

void SDLWindow::start_text_input() {
    SDL_StartTextInput();
}
void SDLWindow::stop_text_input() {
    SDL_StopTextInput();
}