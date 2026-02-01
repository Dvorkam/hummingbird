#pragma once

#include <algorithm>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "layout/Geometry.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout::PaintUtils {

inline void draw_outline(IGraphicsContext& context, const Rect& rect, const Color& color, float thickness = 1.0f) {
    Rect top{rect.x, rect.y, rect.width, thickness};
    Rect bottom{rect.x, rect.y + rect.height - thickness, rect.width, thickness};
    Rect left{rect.x, rect.y, thickness, rect.height};
    Rect right{rect.x + rect.width - thickness, rect.y, thickness, rect.height};
    context.fill_rect(top, color);
    context.fill_rect(bottom, color);
    context.fill_rect(left, color);
    context.fill_rect(right, color);
}

inline float aligned_offset(float available, float size, Css::ComputedStyle::BackgroundPosition::Horizontal align) {
    if (align == Css::ComputedStyle::BackgroundPosition::Horizontal::Center) {
        return (available - size) * 0.5f;
    }
    if (align == Css::ComputedStyle::BackgroundPosition::Horizontal::Right) {
        return available - size;
    }
    return 0.0f;
}

inline float aligned_offset(float available, float size, Css::ComputedStyle::BackgroundPosition::Vertical align) {
    if (align == Css::ComputedStyle::BackgroundPosition::Vertical::Center) {
        return (available - size) * 0.5f;
    }
    if (align == Css::ComputedStyle::BackgroundPosition::Vertical::Bottom) {
        return available - size;
    }
    return 0.0f;
}

inline Rect compute_background_image_rect(const Rect& area, const ImageBitmap& image, const Css::ComputedStyle& style) {
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

inline void draw_background_image(IGraphicsContext& context, const Rect& area, const ImageBitmap& image,
                                  const Css::ComputedStyle& style) {
    Rect dest = compute_background_image_rect(area, image, style);
    if (dest.width <= 0.0f || dest.height <= 0.0f) {
        return;
    }

    if (style.background_repeat == Css::ComputedStyle::BackgroundRepeat::NoRepeat) {
        context.draw_image(image, dest);
        return;
    }

    float start_x = dest.x;
    float start_y = dest.y;
    if (style.background_repeat == Css::ComputedStyle::BackgroundRepeat::Repeat ||
        style.background_repeat == Css::ComputedStyle::BackgroundRepeat::RepeatX) {
        while (start_x > area.x) {
            start_x -= dest.width;
        }
    }
    if (style.background_repeat == Css::ComputedStyle::BackgroundRepeat::Repeat ||
        style.background_repeat == Css::ComputedStyle::BackgroundRepeat::RepeatY) {
        while (start_y > area.y) {
            start_y -= dest.height;
        }
    }

    const bool repeat_y = style.background_repeat == Css::ComputedStyle::BackgroundRepeat::Repeat ||
                          style.background_repeat == Css::ComputedStyle::BackgroundRepeat::RepeatY;
    const bool repeat_x = style.background_repeat == Css::ComputedStyle::BackgroundRepeat::Repeat ||
                          style.background_repeat == Css::ComputedStyle::BackgroundRepeat::RepeatX;
    float y = start_y;
    do {
        float x = start_x;
        do {
            Rect tile{x, y, dest.width, dest.height};
            context.draw_image(image, tile);
            if (!repeat_x) break;
            x += dest.width;
        } while (x < area.x + area.width);
        if (!repeat_y) break;
        y += dest.height;
    } while (y < area.y + area.height);
}

}  // namespace Hummingbird::Layout::PaintUtils
