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

void replay_command(const DisplayCommand& command, IGraphicsContext& context) {
    switch (command.type) {
        case DisplayCommand::Type::FillRect:
            context.fill_rect(command.rect, command.color);
            break;
        case DisplayCommand::Type::DrawImage:
            if (command.image) {
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
    }
}

void draw_outline(IGraphicsContext& context, const Hummingbird::Layout::Rect& rect, const Color& color) {
    Layout::PaintUtils::draw_outline(context, rect, color);
}

}  // namespace Hummingbird::Renderer::RenderCommandUtils
