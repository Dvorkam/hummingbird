#pragma once

#include <cstdint>
#include <string>

enum class EventType : uint8_t {
    None,
    Quit,
    TextInput,
    KeyDown,
    KeyUp,
    MouseDown,
    MouseUp,
    MouseWheel,
    Resize,
};

enum class Key : uint8_t {
    Unknown,

    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    Backspace,
    Enter,
    Escape,
    F1,
};

enum class MouseButton : uint8_t {
    Unknown,
    Left,
    Middle,
    Right,
    X1,
    X2,
};

struct Modifiers {
    bool ctrl{false};
    bool shift{false};
    bool alt{false};
    bool meta{false};  // Windows key / Command key
};

struct InputEvent {
    EventType type{EventType::None};

    // Common modifier state for key-related events
    Modifiers mods{};

    struct {
        Key key{Key::Unknown};
        bool repeat{false};
    } key{};

    struct {
        std::string text;  // UTF-8 (like SDL_TEXTINPUT)
    } text{};

    struct {
        int x{0};
        int y{0};
        MouseButton button{MouseButton::Unknown};
    } mouse_button{};

    struct {
        float dx{0.0f};
        float dy{0.0f};
    } wheel{};

    struct {
        int width{0};
        int height{0};
    } resize{};
};
