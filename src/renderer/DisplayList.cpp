#include "renderer/DisplayList.h"

#include "renderer/RenderCommandUtils.h"

namespace Hummingbird::Renderer {

void DisplayList::add_fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) {
    commands_.push_back(RenderCommandUtils::make_fill_rect(rect, color));
}

void DisplayList::add_draw_image(const ImageBitmap* image, const Hummingbird::Layout::Rect& dest) {
    if (!image) {
        return;
    }
    commands_.push_back(RenderCommandUtils::make_draw_image(image, dest));
}

void DisplayList::add_draw_text(const std::string& text, float x, float y, const TextStyle& style) {
    commands_.push_back(RenderCommandUtils::make_draw_text(text, x, y, style));
}

void DisplayList::add_draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                             const TextMetrics& metrics) {
    commands_.push_back(RenderCommandUtils::make_draw_text_with_metrics(text, x, y, style, metrics));
}

void DisplayList::add_set_global_alpha(float alpha) {
    commands_.push_back(RenderCommandUtils::make_set_global_alpha(alpha));
}

void DisplayList::replay(IGraphicsContext& context) const {
    for (const auto& command : commands_) {
        RenderCommandUtils::replay_command(command, context);
    }
}

}  // namespace Hummingbird::Renderer
