#pragma once

#include <string>
#include <utility>

// Forward declare Rect to break dependency cycle
namespace Hummingbird::Layout {
struct Rect;
}

struct Color {
    unsigned char r, g, b, a;
};

struct TextMetrics {
    float width;
    float height;
};

struct TextStyle {
    std::string font_path;
    float font_size = 16.0f;
    bool bold = false;
    bool italic = false;
    bool monospace = false;
    Color color{0, 0, 0, 255};
};

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;

    virtual void set_viewport(const Hummingbird::Layout::Rect& viewport) = 0;
    virtual void clear(const Color& color) = 0;
    virtual void present() = 0;
    virtual void fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) = 0;
    virtual TextMetrics measure_text(const std::string& text, const TextStyle& style) = 0;
    virtual void draw_text(const std::string& text, float x, float y, const TextStyle& style) = 0;
};
