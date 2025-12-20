#include "layout/TextBox.h"
#include "core/IGraphicsContext.h"
#include "core/AssetPath.h"
#include <iostream>

namespace Hummingbird::Layout {

TextBox::TextBox(const DOM::Text* dom_node) : RenderObject(dom_node) {}

void TextBox::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;

    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;

    const std::string& text = get_dom_node()->get_text();
    if (text.empty()) {
        m_rect.width = padding_left + padding_right;
        m_rect.height = padding_top + padding_bottom;
        return;
    }

    // Assumptions for now: font and size are hardcoded
    auto font_path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    float font_size = 16.0f;

    TextMetrics metrics = context.measure_text(text, font_path.string(), font_size);
    float content_width = metrics.width;
    if (style && style->width.has_value()) {
        content_width = *style->width;
    }

    m_rect.width = content_width + padding_left + padding_right;
    m_rect.height = metrics.height + padding_top + padding_bottom;

    if (metrics.width == 0 || metrics.height == 0) {
        std::cerr << "[TextBox::layout] zero metrics for text '" << text << "' using font " << font_path << "\n";
    }
}

void TextBox::paint(IGraphicsContext& context, const Point& offset) {
    // The absolute position to draw the text is the parent's offset plus our own relative position.
    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;

    float absolute_x = offset.x + m_rect.x + padding_left;
    float absolute_y = offset.y + m_rect.y + padding_top;

    const std::string& text = get_dom_node()->get_text();
    
    // Assumptions for now: font and size are hardcoded
    auto font_path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    float font_size = 16.0f;
    
    context.draw_text(text, absolute_x, absolute_y, font_path.string(), font_size);

    // TextBoxes don't have children, so no need to call base class paint.
}

}
