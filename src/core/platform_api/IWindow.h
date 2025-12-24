#pragma once

#include <memory>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/InputEvent.h"

class IWindow {
public:
    virtual ~IWindow() = default;

    virtual void open() = 0;
    virtual void update() = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;

    virtual std::unique_ptr<IGraphicsContext> get_graphics_context() = 0;
    virtual std::pair<int, int> get_size() const = 0;

    // Input/events (platform-neutral)
    virtual bool wait_event(InputEvent& out, int timeout_ms) = 0;
    virtual bool poll_event(InputEvent& out) = 0;

    // Text input mode (needed for SDL-style TEXTINPUT)
    virtual void start_text_input() = 0;
    virtual void stop_text_input() = 0;
};
