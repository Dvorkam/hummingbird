#pragma once

#include <memory>

#include "core/IGraphicsContext.h"

class IWindow {
public:
    virtual ~IWindow() = default;

    virtual void open() = 0;
    virtual void update() = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;

    virtual std::unique_ptr<IGraphicsContext> get_graphics_context() = 0;
};