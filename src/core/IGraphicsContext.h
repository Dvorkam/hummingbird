#pragma once

struct Color {
    unsigned char r, g, b, a;
};

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;

    virtual void clear(const Color& color) = 0;
    virtual void present() = 0;
};