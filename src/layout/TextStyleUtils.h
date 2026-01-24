#pragma once

#include <algorithm>
#include <string>

#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout::TextStyleUtils {

inline std::string resolve_text_font_path(const Css::ComputedStyle* style) {
    bool bold = style && style->weight == Css::ComputedStyle::FontWeight::Bold;
    bool italic = style && style->style == Css::ComputedStyle::FontStyle::Italic;
    std::string face = style ? style->font_face : std::string{};
    std::transform(face.begin(), face.end(), face.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    bool face_supported = face.empty() || face == "roboto" || face == "sans-serif" || face == "sans serif";
    if (!face_supported) {
        HB_LOG_WARN("[style] Unsupported font face '" << face << "', falling back to Roboto");
    }
    const char* font_path = "assets/fonts/Roboto-Regular.ttf";
    if (bold && italic) {
        font_path = "assets/fonts/Roboto-BoldItalic.ttf";
    } else if (bold) {
        font_path = "assets/fonts/Roboto-Bold.ttf";
    } else if (italic) {
        font_path = "assets/fonts/Roboto-Italic.ttf";
    }
    return Hummingbird::Core::Utils::resolve_asset_path_string(font_path);
}

inline TextStyle build_text_style(const Css::ComputedStyle* style) {
    TextStyle text_style;
    text_style.font_path = resolve_text_font_path(style);
    text_style.font_size = style ? style->font_size : 16.0f;
    text_style.bold = false;
    text_style.italic = false;
    text_style.monospace = style && style->font_monospace;
    text_style.color = style ? style->color : Color{0, 0, 0, 255};
    return text_style;
}

}  // namespace Hummingbird::Layout::TextStyleUtils
