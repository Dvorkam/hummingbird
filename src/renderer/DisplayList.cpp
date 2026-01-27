#include "renderer/DisplayList.h"

namespace Hummingbird::Renderer {

void DisplayList::add_fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::FillRect;
    command.rect = rect;
    command.color = color;
    commands_.push_back(std::move(command));
}

void DisplayList::add_draw_image(const ImageBitmap* image, const Hummingbird::Layout::Rect& dest) {
    if (!image) {
        return;
    }
    DisplayCommand command;
    command.type = DisplayCommand::Type::DrawImage;
    command.rect = dest;
    command.image = image;
    commands_.push_back(std::move(command));
}

void DisplayList::add_draw_text(const std::string& text, float x, float y, const TextStyle& style) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::DrawText;
    command.text = text;
    command.x = x;
    command.y = y;
    command.text_style = style;
    commands_.push_back(std::move(command));
}

void DisplayList::add_draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                             const TextMetrics& metrics) {
    DisplayCommand command;
    command.type = DisplayCommand::Type::DrawTextWithMetrics;
    command.text = text;
    command.x = x;
    command.y = y;
    command.text_style = style;
    command.text_metrics = metrics;
    commands_.push_back(std::move(command));
}

void DisplayList::replay(IGraphicsContext& context) const {
    for (const auto& command : commands_) {
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
        }
    }
}

}  // namespace Hummingbird::Renderer
