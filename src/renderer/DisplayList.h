#pragma once

#include <string>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
struct ImageBitmap;
}

namespace Hummingbird::Renderer {

struct DisplayCommand {
    enum class Type {
        FillRect,
        DrawImage,
        DrawText,
        DrawTextWithMetrics,
        SetGlobalAlpha,
        PushClip,
        PopClip,
    };

    Type type;
    Hummingbird::Layout::Rect rect{};
    Color color{};
    const ImageBitmap* image = nullptr;
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    TextStyle text_style{};
    TextMetrics text_metrics{};
    float alpha = 1.0f;
};

class DisplayList {
public:
    void clear() { commands_.clear(); }
    size_t size() const { return commands_.size(); }

    void add_fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color);
    void add_draw_image(const ImageBitmap* image, const Hummingbird::Layout::Rect& dest);
    void add_draw_text(const std::string& text, float x, float y, const TextStyle& style);
    void add_draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                    const TextMetrics& metrics);
    void add_set_global_alpha(float alpha);
    void add_push_clip(const Hummingbird::Layout::Rect& rect);
    void add_pop_clip();

    void replay(IGraphicsContext& context) const;

private:
    std::vector<DisplayCommand> commands_;
};

class DisplayListRecorder final : public IGraphicsContext {
public:
    DisplayListRecorder(DisplayList& list, IGraphicsContext& metrics_source)
        : list_(list), metrics_source_(metrics_source) {}

    void set_viewport(const Hummingbird::Layout::Rect& viewport) override { viewport_ = viewport; }
    void clear(const Color& /*color*/) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) override {
        list_.add_fill_rect(rect, color);
    }
    void draw_image(const ImageBitmap& image, const Hummingbird::Layout::Rect& dest) override {
        list_.add_draw_image(&image, dest);
    }

    TextMetrics measure_text(const std::string& text, const TextStyle& style) override {
        return metrics_source_.measure_text(text, style);
    }

    void draw_text(const std::string& text, float x, float y, const TextStyle& style) override {
        list_.add_draw_text(text, x, y, style);
    }

    void draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                const TextMetrics& metrics) override {
        list_.add_draw_text_with_metrics(text, x, y, style, metrics);
    }
    void set_global_alpha(float alpha) override { list_.add_set_global_alpha(alpha); }
    void push_clip(const Hummingbird::Layout::Rect& rect) override { list_.add_push_clip(rect); }
    void pop_clip() override { list_.add_pop_clip(); }

private:
    DisplayList& list_;
    IGraphicsContext& metrics_source_;
    Hummingbird::Layout::Rect viewport_{0, 0, 0, 0};
};

}  // namespace Hummingbird::Renderer
