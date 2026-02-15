#pragma once

#include <string_view>

namespace Hummingbird {
class IGraphicsContext;
struct TextStyle;
}  // namespace Hummingbird

namespace Hummingbird::Layout {
class TextMeasurer {
public:
    TextMeasurer(IGraphicsContext& context, const TextStyle& text_style, float letter_spacing);
    float measure(std::string_view text) const;

private:
    IGraphicsContext& context_;
    const TextStyle& text_style_;
    float letter_spacing_ = 0.0f;
};
}  // namespace Hummingbird::Layout
