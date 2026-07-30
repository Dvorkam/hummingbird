#pragma once

#include <vector>

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
    void draw_image(ResourceRef /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override {}
    void draw_image(const ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override {}

    // Clip calls are recorded so tests can assert overflow:hidden clipping
    // (story 8.5.3). `pushed_clips` holds each push_clip rect in order; the two
    // counters track the raw push/pop calls.
    void push_clip(const Hummingbird::Layout::Rect& rect) override {
        pushed_clips.push_back(rect);
        ++push_clip_count;
    }
    void pop_clip() override { ++pop_clip_count; }

    std::vector<Hummingbird::Layout::Rect> pushed_clips;
    int push_clip_count = 0;
    int pop_clip_count = 0;

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

    // Painted text is recorded so a test can assert what actually reached the
    // screen, rather than what the DOM says. The two disagree exactly when a
    // rebuild is missed, which is a whole class of bug that DOM-level
    // assertions cannot see.
    void draw_text(const std::string& text, float /*x*/, float /*y*/, const TextStyle& /*style*/) override {
        drawn_texts.push_back(text);
    }

    std::vector<std::string> drawn_texts;
};

}  // namespace Hummingbird::Test
