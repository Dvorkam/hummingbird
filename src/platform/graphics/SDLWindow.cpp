#include "platform/graphics/SDLWindow.h"

#include <SDL.h>
#include <SDL_clipboard.h>
#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_image.h>
#include <SDL_keyboard.h>
#include <SDL_render.h>
#include <SDL_stdinc.h>
#include <SDL_surface.h>
#include <SDL_video.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "platform/graphics/SDLGraphicsContext.h"
#include "platform/graphics/SDLInputTranslation.h"

namespace Hummingbird::Platform {

namespace {
bool env_truthy(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !value[0]) {
        return false;
    }
    std::string_view raw(value);
    std::string normalized;
    normalized.reserve(raw.size());
    std::transform(raw.begin(), raw.end(), std::back_inserter(normalized),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

SDL_Surface* load_window_icon_surface() {
    static bool sdl_image_ready = false;
    if (!sdl_image_ready) {
        int initted = IMG_Init(IMG_INIT_PNG);
        if ((initted & IMG_INIT_PNG) != IMG_INIT_PNG) {
            HB_LOG_WARN("[platform] SDL2_image PNG support not available: " << IMG_GetError());
        }
        sdl_image_ready = true;
    }

    static const char* kCandidates[] = {
        "assets/icons/hummingbird-32.png",
        "assets/icons/hummingbird-16.png",
        "assets/icons/hummingbird-256.png",
    };

    for (const char* candidate : kCandidates) {
        const std::string& path = Hummingbird::Core::Utils::resolve_asset_path_string(candidate);
        SDL_Surface* icon = IMG_Load(path.c_str());
        if (icon) {
            return icon;
        }
    }

    HB_LOG_WARN("[platform] Failed to load window icon from assets/icons");
    return nullptr;
}
}  // namespace

SDLWindow::SDLWindow() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        HB_LOG_ERROR("[platform] SDL_Init failed: " << SDL_GetError());
    }
}

SDLWindow::~SDLWindow() {
    close();
    SDL_Quit();
}

void SDLWindow::open() {
    const bool headless = env_truthy("HB_HEADLESS");
    const Uint32 window_flags = headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;

    m_window = SDL_CreateWindow("Hummingbird", 100, 100, 1024, 768, window_flags);
    if (m_window == nullptr) {
        HB_LOG_ERROR("[platform] SDL_CreateWindow failed: " << SDL_GetError());
        return;
    }

    if (!headless) {
        if (SDL_Surface* icon = load_window_icon_surface()) {
            SDL_SetWindowIcon(m_window, icon);
            SDL_FreeSurface(icon);
        }
    }

    if (headless) {
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    } else {
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    }
    if (m_renderer == nullptr && !headless) {
        HB_LOG_WARN("[platform] SDL_CreateRenderer (accelerated) failed: " << SDL_GetError());
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (m_renderer == nullptr && headless) {
        HB_LOG_WARN("[platform] SDL_CreateRenderer (software headless) failed: " << SDL_GetError());
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (m_renderer == nullptr) {
        HB_LOG_ERROR("[platform] SDL_CreateRenderer failed: " << SDL_GetError());
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

bool SDLWindow::wait_event(InputEvent& out, int timeout_ms) {
    SDL_Event e;
    if (!SDL_WaitEventTimeout(&e, timeout_ms)) return false;
    return Hummingbird::Platform::SDLInput::translate_event(e, out);
}

bool SDLWindow::poll_event(InputEvent& out) {
    SDL_Event e;
    if (!SDL_PollEvent(&e)) return false;
    return Hummingbird::Platform::SDLInput::translate_event(e, out);
}

void SDLWindow::start_text_input() {
    SDL_StartTextInput();
}
void SDLWindow::stop_text_input() {
    SDL_StopTextInput();
}

std::string SDLWindow::get_clipboard_text() const {
    if (!SDL_HasClipboardText()) return {};

    char* text = SDL_GetClipboardText();
    if (!text) {
        return {};
    }

    std::string out = text;
    SDL_free(text);
    return out;
}

}  // namespace Hummingbird::Platform
