#include "layout/flow/TextOverflowUtils.h"

#include <string>

#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/Utf8Utils.h"
#include "layout/flow/TextMeasurer.h"

namespace Hummingbird::Layout::TextOverflowUtils {

std::string ellipsize_text_to_width(IGraphicsContext& context, std::string_view text, const TextStyle& text_style,
                                    float letter_spacing, float max_width) {
    if (max_width <= 0.0f) {
        return "";
    }
    constexpr std::string_view kEllipsis = "...";
    TextMeasurer measurer(context, text_style, letter_spacing);
    float ellipsis_width = measurer.measure(kEllipsis);
    if (ellipsis_width >= max_width) {
        return std::string(kEllipsis);
    }

    TextMeasurer glyph_measurer(context, text_style, 0.0f);
    std::string out;
    float width = 0.0f;
    size_t index = 0;
    while (index < text.size()) {
        size_t next = Core::Utils::next_codepoint(text, index);
        if (next <= index) {
            break;
        }
        std::string_view glyph_view = text.substr(index, next - index);
        float glyph_width = glyph_measurer.measure(glyph_view);
        float spacing_after = out.empty() ? 0.0f : letter_spacing;
        if (width + spacing_after + glyph_width + letter_spacing + ellipsis_width > max_width) {
            break;
        }
        if (!out.empty()) {
            width += letter_spacing;
        }
        out.append(glyph_view.begin(), glyph_view.end());
        width += glyph_width;
        index = next;
    }
    out += std::string(kEllipsis);
    return out;
}

}  // namespace Hummingbird::Layout::TextOverflowUtils
