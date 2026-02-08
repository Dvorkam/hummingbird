#pragma once

#include "core/platform_api/IGraphicsContext.h"

namespace Hummingbird::Test {

// A lightweight graphics context used in tests. All operations are no-ops
// except text measurement, which uses a simple heuristic to return stable values.
class TestGraphicsContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& /*viewport*/) override {}
    void clear(const Color& /*color*/) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& /*rect*/, const Color& /*color*/) override {}
    void draw_image(const ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override {}

    TextMetrics measure_text(const std::string& text, const TextStyle& style) override {
        // Approximate metrics based on character count to keep tests deterministic.
        const float font_size = style.font_size > 0.0f ? style.font_size : 16.0f;
        const float average_char_width = font_size * 0.5f;
        TextMetrics metrics;
        metrics.width = static_cast<float>(text.size()) * average_char_width;
        metrics.height = font_size;
        metrics.ascent = font_size * 0.8f;
        metrics.descent = font_size * 0.2f;
        metrics.underline_position = -metrics.descent * 0.5f;
        metrics.underline_thickness = 1.0f;
        return metrics;
    }

    void draw_text(const std::string& /*text*/, float /*x*/, float /*y*/, const TextStyle& /*style*/) override {}
};

}  // namespace Hummingbird::Test
