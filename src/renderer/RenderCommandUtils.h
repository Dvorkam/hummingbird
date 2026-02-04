#pragma once

#include <string>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::Renderer {

struct DisplayCommand;

namespace RenderCommandUtils {

DisplayCommand make_fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color);
DisplayCommand make_draw_image(const ImageBitmap* image, const Hummingbird::Layout::Rect& dest);
DisplayCommand make_draw_text(const std::string& text, float x, float y, const TextStyle& style);
DisplayCommand make_draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                           const TextMetrics& metrics);
void replay_command(const DisplayCommand& command, IGraphicsContext& context);
void draw_outline(IGraphicsContext& context, const Hummingbird::Layout::Rect& rect, const Color& color);

}  // namespace RenderCommandUtils

}  // namespace Hummingbird::Renderer
