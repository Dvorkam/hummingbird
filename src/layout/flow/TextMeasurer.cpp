#include "layout/flow/TextMeasurer.h"

#include <string>

#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/Utf8Utils.h"

namespace Hummingbird::Layout {

TextMeasurer::TextMeasurer(IGraphicsContext& context, const TextStyle& text_style, float letter_spacing)
    : context_(context), text_style_(text_style), letter_spacing_(letter_spacing) {}

float TextMeasurer::measure(std::string_view text) const {
    if (text.empty()) {
        return 0.0f;
    }
    float width = 0.0f;
    size_t index = 0;
    size_t codepoint_count = 0;
    while (index < text.size()) {
        size_t next = Core::Utils::next_codepoint(text, index);
        if (next <= index) {
            break;
        }
        std::string glyph(text.substr(index, next - index));
        width += context_.measure_text(glyph, text_style_).width;
        ++codepoint_count;
        index = next;
    }
    if (codepoint_count > 1 && letter_spacing_ != 0.0f) {
        width += letter_spacing_ * static_cast<float>(codepoint_count - 1);
    }
    return width;
}

}  // namespace Hummingbird::Layout
