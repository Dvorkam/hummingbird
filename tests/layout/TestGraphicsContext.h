#pragma once

#include "core/IGraphicsContext.h"

// A lightweight graphics context used in tests. All operations are no-ops
// except text measurement, which uses a simple heuristic to return stable values.
class TestGraphicsContext : public IGraphicsContext {
public:
    void clear(const Color& /*color*/) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& /*rect*/, const Color& /*color*/) override {}

    TextMetrics measure_text(const std::string& text, const TextStyle& /*style*/) override {
        // Approximate metrics based on character count to keep tests deterministic.
        constexpr float kAverageCharWidth = 8.0f;
        constexpr float kLineHeight = 16.0f;
        return TextMetrics{
            .width = static_cast<float>(text.size()) * kAverageCharWidth,
            .height = kLineHeight
        };
    }

    void draw_text(const std::string& /*text*/, float /*x*/, float /*y*/, const TextStyle& /*style*/) override {}
};
