#pragma once

#include <string>
#include <string_view>

namespace Hummingbird {
class IGraphicsContext;
struct TextStyle;
}  // namespace Hummingbird

namespace Hummingbird::Layout::TextOverflowUtils {
std::string ellipsize_text_to_width(IGraphicsContext& context, std::string_view text, const TextStyle& text_style,
                                    float letter_spacing, float max_width);
}
