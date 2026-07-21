#include "platform/graphics/SDLInputTranslation.h"

#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <SDL_video.h>
#include <stdint.h>

#include <string>

#include "core/platform_api/InputEvent.h"

namespace Hummingbird::Platform::SDLInput {

namespace detail {
Modifiers to_mods(SDL_Keymod mod) {
    Modifiers m;
    m.ctrl = (mod & KMOD_CTRL) != 0;
    m.shift = (mod & KMOD_SHIFT) != 0;
    m.alt = (mod & KMOD_ALT) != 0;
    m.meta = (mod & KMOD_GUI) != 0;
    return m;
}

Key to_key(SDL_Keycode kc) {
    if (kc >= SDLK_a && kc <= SDLK_z) {
        return static_cast<Key>(static_cast<uint8_t>(Key::A) + static_cast<uint8_t>(kc - SDLK_a));
    }
    // SDLK_0..SDLK_9 are the contiguous ASCII digits '0'..'9'.
    if (kc >= SDLK_0 && kc <= SDLK_9) {
        return static_cast<Key>(static_cast<uint8_t>(Key::Num0) + static_cast<uint8_t>(kc - SDLK_0));
    }
    switch (kc) {
        case SDLK_SPACE:
            return Key::Space;
        case SDLK_MINUS:
            return Key::Minus;
        case SDLK_EQUALS:
            return Key::Equals;
        case SDLK_LEFTBRACKET:
            return Key::LeftBracket;
        case SDLK_RIGHTBRACKET:
            return Key::RightBracket;
        case SDLK_BACKSLASH:
            return Key::Backslash;
        case SDLK_SEMICOLON:
            return Key::Semicolon;
        case SDLK_QUOTE:
            return Key::Quote;
        case SDLK_BACKQUOTE:
            return Key::Backquote;
        case SDLK_COMMA:
            return Key::Comma;
        case SDLK_PERIOD:
            return Key::Period;
        case SDLK_SLASH:
            return Key::Slash;
        case SDLK_TAB:
            return Key::Tab;
        case SDLK_BACKSPACE:
            return Key::Backspace;
        case SDLK_DELETE:
            return Key::Delete;
        case SDLK_INSERT:
            return Key::Insert;
        case SDLK_HOME:
            return Key::Home;
        case SDLK_END:
            return Key::End;
        case SDLK_LEFT:
            return Key::Left;
        case SDLK_RIGHT:
            return Key::Right;
        case SDLK_RETURN:
            return Key::Enter;
        case SDLK_ESCAPE:
            return Key::Escape;
        case SDLK_F1:
            return Key::F1;
        case SDLK_F5:
            return Key::F5;
        default:
            return Key::Unknown;
    }
}

MouseButton to_mouse_button(uint8_t b) {
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

bool translate_text_input(const SDL_Event& e, InputEvent& out) {
    out.type = EventType::TextInput;
    out.text.text = e.text.text;  // UTF-8
    return true;
}

bool translate_key_event(const SDL_Event& e, InputEvent& out, EventType type, bool repeat) {
    out.type = type;
    out.mods = to_mods(static_cast<SDL_Keymod>(e.key.keysym.mod));
    out.key.key = to_key(e.key.keysym.sym);
    out.key.repeat = repeat;
    return true;
}

bool translate_mouse_button_event(const SDL_Event& e, InputEvent& out, EventType type) {
    out.type = type;
    out.mouse_button.x = e.button.x;
    out.mouse_button.y = e.button.y;
    out.mouse_button.button = to_mouse_button(e.button.button);
    out.mouse_button.clicks = e.button.clicks;  // 2 on the second click of a double
    return true;
}

bool translate_mouse_wheel_event(const SDL_Event& e, InputEvent& out) {
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

bool translate_window_event(const SDL_Event& e, InputEvent& out) {
    if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED) {
        out.type = EventType::Resize;
        out.resize.width = e.window.data1;
        out.resize.height = e.window.data2;
        return true;
    }
    return false;
}
}  // namespace detail

bool translate_event(const SDL_Event& e, InputEvent& out) {
    out = {};  // reset

    switch (e.type) {
        case SDL_QUIT:
            out.type = EventType::Quit;
            return true;

        case SDL_TEXTINPUT:
            return detail::translate_text_input(e, out);

        case SDL_KEYDOWN:
            return detail::translate_key_event(e, out, EventType::KeyDown, e.key.repeat != 0);

        case SDL_KEYUP:
            return detail::translate_key_event(e, out, EventType::KeyUp, false);

        case SDL_MOUSEBUTTONDOWN:
            return detail::translate_mouse_button_event(e, out, EventType::MouseDown);

        case SDL_MOUSEBUTTONUP:
            return detail::translate_mouse_button_event(e, out, EventType::MouseUp);

        case SDL_MOUSEWHEEL:
            return detail::translate_mouse_wheel_event(e, out);

        case SDL_WINDOWEVENT:
            return detail::translate_window_event(e, out);

        default:
            return false;
    }
}

}  // namespace Hummingbird::Platform::SDLInput
