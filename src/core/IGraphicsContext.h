#pragma once

#include <string>

// Forward declare Rect to break dependency cycle
namespace Hummingbird::Layout { struct Rect; }

struct Color {
    unsigned char r, g, b, a;
};

struct TextMetrics {
    float width;
    float height;
};

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;

    virtual void clear(const Color& color) = 0;
    virtual void present() = 0;
    virtual void fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) = 0;
    virtual TextMetrics measure_text(const std::string& text, const std::string& font_path, float font_size) = 0;
    virtual void draw_text(const std::string& text, float x, float y, const std::string& font_path, float font_size) = 0;
};