#include "renderer/RenderCommandUtils.h"

#include "layout/paint/PaintUtils.h"
#include "renderer/DisplayList.h"

namespace Hummingbird::Renderer::RenderCommandUtils {

DisplayCommand make_fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::FillRect;
    command.rect = rect;
    command.color = color;
    return command;
}

DisplayCommand make_draw_image(ResourceRef image, const Hummingbird::Layout::Rect& dest) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::DrawImage;
    command.rect = dest;
    command.image_ref = image;
    return command;
}

DisplayCommand make_draw_image(const ImageBitmap* image, const Hummingbird::Layout::Rect& dest) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::DrawImage;
    command.rect = dest;
    command.image = image;
    return command;
}

DisplayCommand make_draw_text(const std::string& text, float x, float y, const TextStyle& style) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::DrawText;
    command.text = text;
    command.x = x;
    command.y = y;
    command.text_style = style;
    return command;
}

DisplayCommand make_draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                           const TextMetrics& metrics) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::DrawTextWithMetrics;
    command.text = text;
    command.x = x;
    command.y = y;
    command.text_style = style;
    command.text_metrics = metrics;
    return command;
}

DisplayCommand make_set_global_alpha(float alpha) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::SetGlobalAlpha;
    command.alpha = alpha;
    return command;
}

DisplayCommand make_push_clip(const Hummingbird::Layout::Rect& rect) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::PushClip;
    command.rect = rect;
    return command;
}

DisplayCommand make_pop_clip() {
    DisplayCommand command;
    command.type = DisplayCommand::Type::PopClip;
    return command;
}

void replay_command(const DisplayCommand& command, IGraphicsContext& context) {
    switch (command.type) {
        case DisplayCommand::Type::FillRect:
            context.fill_rect(command.rect, command.color);
            break;
        case DisplayCommand::Type::DrawImage:
            // A reference is resolved by the context, at replay time, so a
            // resource freed since this list was recorded simply draws nothing.
            if (command.image_ref.valid()) {
                context.draw_image(command.image_ref, command.rect);
            } else if (command.image) {
                context.draw_image(*command.image, command.rect);
            }
            break;
        case DisplayCommand::Type::DrawText:
            context.draw_text(command.text, command.x, command.y, command.text_style);
            break;
        case DisplayCommand::Type::DrawTextWithMetrics:
            context.draw_text_with_metrics(command.text, command.x, command.y, command.text_style,
                                           command.text_metrics);
            break;
        case DisplayCommand::Type::SetGlobalAlpha:
            context.set_global_alpha(command.alpha);
            break;
        case DisplayCommand::Type::PushClip:
            context.push_clip(command.rect);
            break;
        case DisplayCommand::Type::PopClip:
            context.pop_clip();
            break;
    }
}

void draw_outline(IGraphicsContext& context, const Hummingbird::Layout::Rect& rect, const Color& color) {
    Layout::PaintUtils::draw_outline(context, rect, color);
}

}  // namespace Hummingbird::Renderer::RenderCommandUtils
