#include "layout/RenderObject.h"

#include <algorithm>
#include <optional>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
float aligned_offset(float available, float size, Css::ComputedStyle::BackgroundPosition::Horizontal align) {
    if (align == Css::ComputedStyle::BackgroundPosition::Horizontal::Center) {
        return (available - size) * 0.5f;
    }
    if (align == Css::ComputedStyle::BackgroundPosition::Horizontal::Right) {
        return available - size;
    }
    return 0.0f;
}

float aligned_offset(float available, float size, Css::ComputedStyle::BackgroundPosition::Vertical align) {
    if (align == Css::ComputedStyle::BackgroundPosition::Vertical::Center) {
        return (available - size) * 0.5f;
    }
    if (align == Css::ComputedStyle::BackgroundPosition::Vertical::Bottom) {
        return available - size;
    }
    return 0.0f;
}

Rect background_image_rect(const Rect& area, const ImageBitmap& image, const Css::ComputedStyle& style) {
    float image_width = static_cast<float>(image.width);
    float image_height = static_cast<float>(image.height);
    if (image_width <= 0.0f || image_height <= 0.0f) {
        return {area.x, area.y, 0.0f, 0.0f};
    }

    float dest_width = image_width;
    float dest_height = image_height;
    if (style.background_size.type == Css::ComputedStyle::BackgroundSize::Type::Contain ||
        style.background_size.type == Css::ComputedStyle::BackgroundSize::Type::Cover) {
        float scale_x = area.width / image_width;
        float scale_y = area.height / image_height;
        float scale = style.background_size.type == Css::ComputedStyle::BackgroundSize::Type::Cover
                          ? std::max(scale_x, scale_y)
                          : std::min(scale_x, scale_y);
        dest_width = image_width * scale;
        dest_height = image_height * scale;
    } else if (style.background_size.type == Css::ComputedStyle::BackgroundSize::Type::Length) {
        if (style.background_size.width) {
            dest_width = *style.background_size.width;
        }
        if (style.background_size.height) {
            dest_height = *style.background_size.height;
        }
    }

    float offset_x = style.background_position.offset_x.value_or(
        aligned_offset(area.width, dest_width, style.background_position.horizontal));
    float offset_y = style.background_position.offset_y.value_or(
        aligned_offset(area.height, dest_height, style.background_position.vertical));
    return {area.x + offset_x, area.y + offset_y, dest_width, dest_height};
}
}  // namespace

void RenderObject::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect = bounds;
}

void RenderObject::paint(IGraphicsContext& context, const Point& offset) const {
    paint_self(context, offset);
    Point child_offset = {offset.x + m_rect.x, offset.y + m_rect.y};
    for (auto& child : m_children) {
        child->paint(context, child_offset);
    }
}

void RenderObject::paint_self(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    if (style && style->background.has_value()) {
        Layout::Rect background{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
        context.fill_rect(background, *style->background);
    }
    if (style && m_background_image && style->background_image.has_value()) {
        Layout::Rect area{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
        Rect dest = background_image_rect(area, *m_background_image, *style);
        if (dest.width > 0.0f && dest.height > 0.0f) {
            if (style->background_repeat == Css::ComputedStyle::BackgroundRepeat::NoRepeat) {
                context.draw_image(*m_background_image, dest);
            } else {
                float start_x = dest.x;
                float start_y = dest.y;
                if (style->background_repeat == Css::ComputedStyle::BackgroundRepeat::Repeat ||
                    style->background_repeat == Css::ComputedStyle::BackgroundRepeat::RepeatX) {
                    while (start_x > area.x) {
                        start_x -= dest.width;
                    }
                }
                if (style->background_repeat == Css::ComputedStyle::BackgroundRepeat::Repeat ||
                    style->background_repeat == Css::ComputedStyle::BackgroundRepeat::RepeatY) {
                    while (start_y > area.y) {
                        start_y -= dest.height;
                    }
                }

                float y = start_y;
                const bool repeat_y = style->background_repeat == Css::ComputedStyle::BackgroundRepeat::Repeat ||
                                      style->background_repeat == Css::ComputedStyle::BackgroundRepeat::RepeatY;
                const bool repeat_x = style->background_repeat == Css::ComputedStyle::BackgroundRepeat::Repeat ||
                                      style->background_repeat == Css::ComputedStyle::BackgroundRepeat::RepeatX;
                do {
                    float x = start_x;
                    do {
                        Rect tile{x, y, dest.width, dest.height};
                        context.draw_image(*m_background_image, tile);
                        if (!repeat_x) break;
                        x += dest.width;
                    } while (x < area.x + area.width);
                    if (!repeat_y) break;
                    y += dest.height;
                } while (y < area.y + area.height);
            }
        }
    }
    if (style && style->border_style != Css::ComputedStyle::BorderStyle::None) {
        Layout::Rect absolute{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
        const auto& bw = style->border_width;
        const auto& color = style->border_color;

        if (bw.top > 0.0f) {
            Layout::Rect top{absolute.x, absolute.y, absolute.width, bw.top};
            context.fill_rect(top, color);
        }
        if (bw.bottom > 0.0f) {
            Layout::Rect bottom{absolute.x, absolute.y + absolute.height - bw.bottom, absolute.width, bw.bottom};
            context.fill_rect(bottom, color);
        }
        if (bw.left > 0.0f) {
            Layout::Rect left{absolute.x, absolute.y, bw.left, absolute.height};
            context.fill_rect(left, color);
        }
        if (bw.right > 0.0f) {
            Layout::Rect right{absolute.x + absolute.width - bw.right, absolute.y, bw.right, absolute.height};
            context.fill_rect(right, color);
        }
    }
}

}  // namespace Hummingbird::Layout
