#pragma once

#include <algorithm>
#include <cmath>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "layout/geometry/Geometry.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout::PaintUtils {

inline void draw_outline(IGraphicsContext& context, const Rect& rect, const Color& color, float thickness = 1.0f);

inline float scanline_inset(float y_local, float height, float radius) {
    if (radius <= 0.0f || height <= 0.0f) {
        return 0.0f;
    }
    if (y_local < 0.0f || y_local >= height) {
        return 0.0f;
    }

    const float r = std::min(radius, height * 0.5f);
    if (y_local < r) {
        const float dy = r - (y_local + 0.5f);
        return r - std::sqrt(std::max(0.0f, r * r - dy * dy));
    }
    if (y_local >= height - r) {
        const float dy = (y_local - (height - r)) + 0.5f;
        return r - std::sqrt(std::max(0.0f, r * r - dy * dy));
    }
    return 0.0f;
}

inline void draw_rounded_fill(IGraphicsContext& context, const Rect& rect, float radius, const Color& color) {
    const float clamped_radius = std::max(0.0f, std::min(radius, std::min(rect.width, rect.height) * 0.5f));
    if (clamped_radius <= 0.0f) {
        context.fill_rect(rect, color);
        return;
    }

    const int row_count = std::max(1, static_cast<int>(std::ceil(rect.height)));
    for (int row = 0; row < row_count; ++row) {
        const float y = rect.y + static_cast<float>(row);
        const float inset = scanline_inset(static_cast<float>(row), rect.height, clamped_radius);
        const float x = rect.x + inset;
        const float width = std::max(0.0f, rect.width - inset * 2.0f);
        if (width <= 0.0f) {
            continue;
        }
        context.fill_rect(Rect{x, y, width, 1.0f}, color);
    }
}

// Corner radii resolved to px for a specific box, clamped so no single corner
// exceeds half the shorter side.
struct ResolvedCorners {
    float top_left = 0.0f;
    float top_right = 0.0f;
    float bottom_right = 0.0f;
    float bottom_left = 0.0f;
    bool any() const {
        return top_left > 0.0f || top_right > 0.0f || bottom_right > 0.0f || bottom_left > 0.0f;
    }
};

inline ResolvedCorners resolve_corners(const Css::CornerRadii& radii, float width, float height) {
    const float reference = std::min(width, height);
    const float cap = std::max(0.0f, reference * 0.5f);
    auto rv = [&](const Css::CornerRadius& corner) {
        return std::min(std::max(0.0f, corner.resolve(reference)), cap);
    };
    return {rv(radii.top_left), rv(radii.top_right), rv(radii.bottom_right), rv(radii.bottom_left)};
}

// Horizontal inset into a circular corner of the given radius, for a row whose
// center is `dist_from_edge` px from the near horizontal edge.
inline float corner_inset(float dist_from_edge, float radius) {
    if (radius <= 0.0f || dist_from_edge >= radius) {
        return 0.0f;
    }
    const float dy = radius - dist_from_edge;
    return radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy));
}

// Left/right horizontal insets for one scanline row, given four corner radii.
inline void corner_insets_at_row(const ResolvedCorners& corners, float row_center, float height, float& left,
                                 float& right) {
    const float dist_top = row_center;
    const float dist_bottom = height - row_center;
    left = std::max(corner_inset(dist_top, corners.top_left), corner_inset(dist_bottom, corners.bottom_left));
    right = std::max(corner_inset(dist_top, corners.top_right), corner_inset(dist_bottom, corners.bottom_right));
}

// Scanline fill supporting four independent corner radii (e.g. `0 4px 4px 0`).
inline void draw_rounded_fill_corners(IGraphicsContext& context, const Rect& rect, const ResolvedCorners& corners,
                                      const Color& color) {
    if (!corners.any()) {
        context.fill_rect(rect, color);
        return;
    }
    const int row_count = std::max(1, static_cast<int>(std::ceil(rect.height)));
    for (int row = 0; row < row_count; ++row) {
        float left = 0.0f;
        float right = 0.0f;
        corner_insets_at_row(corners, static_cast<float>(row) + 0.5f, rect.height, left, right);
        const float width = std::max(0.0f, rect.width - left - right);
        if (width > 0.0f) {
            context.fill_rect(Rect{rect.x + left, rect.y + static_cast<float>(row), width, 1.0f}, color);
        }
    }
}

// Uniform-width border stroke that follows four independent corner radii: fills
// the gap between the outer rounded edge and an inner edge inset by `thickness`
// (with each corner radius reduced by the thickness).
inline void draw_rounded_border_corners(IGraphicsContext& context, const Rect& rect, const ResolvedCorners& corners,
                                        float thickness, const Color& color) {
    if (thickness <= 0.0f) {
        return;
    }
    const Rect inner{rect.x + thickness, rect.y + thickness, std::max(0.0f, rect.width - thickness * 2.0f),
                     std::max(0.0f, rect.height - thickness * 2.0f)};
    const ResolvedCorners inner_corners{
        std::max(0.0f, corners.top_left - thickness),
        std::max(0.0f, corners.top_right - thickness),
        std::max(0.0f, corners.bottom_right - thickness),
        std::max(0.0f, corners.bottom_left - thickness),
    };
    const int row_count = std::max(1, static_cast<int>(std::ceil(rect.height)));
    for (int row = 0; row < row_count; ++row) {
        const float y_abs = rect.y + static_cast<float>(row);
        float outer_left = 0.0f;
        float outer_right = 0.0f;
        corner_insets_at_row(corners, static_cast<float>(row) + 0.5f, rect.height, outer_left, outer_right);
        const float ox_left = rect.x + outer_left;
        const float ox_right = rect.x + rect.width - outer_right;
        if (ox_right <= ox_left) {
            continue;
        }
        if (y_abs < inner.y || y_abs >= inner.y + inner.height || inner.width <= 0.0f || inner.height <= 0.0f) {
            context.fill_rect(Rect{ox_left, y_abs, ox_right - ox_left, 1.0f}, color);
            continue;
        }
        float inner_left = 0.0f;
        float inner_right = 0.0f;
        corner_insets_at_row(inner_corners, (y_abs - inner.y) + 0.5f, inner.height, inner_left, inner_right);
        const float ix_left = inner.x + inner_left;
        const float ix_right = inner.x + inner.width - inner_right;
        if (ix_left > ox_left) {
            context.fill_rect(Rect{ox_left, y_abs, ix_left - ox_left, 1.0f}, color);
        }
        if (ox_right > ix_right) {
            context.fill_rect(Rect{ix_right, y_abs, ox_right - ix_right, 1.0f}, color);
        }
    }
}

inline bool is_uniform_border_width(const Css::EdgeSizes& width, float epsilon = 0.01f) {
    return std::fabs(width.top - width.right) < epsilon && std::fabs(width.top - width.bottom) < epsilon &&
           std::fabs(width.top - width.left) < epsilon;
}

inline void draw_uniform_rounded_border(IGraphicsContext& context, const Rect& rect, float radius, float thickness,
                                        const Color& color) {
    if (thickness <= 0.0f) {
        return;
    }
    const float clamped_radius = std::max(0.0f, std::min(radius, std::min(rect.width, rect.height) * 0.5f));
    if (clamped_radius <= 0.0f) {
        draw_outline(context, rect, color, thickness);
        return;
    }

    const Rect inner{rect.x + thickness, rect.y + thickness, std::max(0.0f, rect.width - thickness * 2.0f),
                     std::max(0.0f, rect.height - thickness * 2.0f)};
    const float inner_radius = std::max(0.0f, clamped_radius - thickness);
    const int row_count = std::max(1, static_cast<int>(std::ceil(rect.height)));

    for (int row = 0; row < row_count; ++row) {
        const float y_abs = rect.y + static_cast<float>(row);
        const float outer_inset = scanline_inset(static_cast<float>(row), rect.height, clamped_radius);
        const float outer_left = rect.x + outer_inset;
        const float outer_right = rect.x + rect.width - outer_inset;
        if (outer_right <= outer_left) {
            continue;
        }

        if (y_abs < inner.y || y_abs >= inner.y + inner.height || inner.width <= 0.0f || inner.height <= 0.0f) {
            context.fill_rect(Rect{outer_left, y_abs, outer_right - outer_left, 1.0f}, color);
            continue;
        }

        const float inner_y_local = y_abs - inner.y;
        const float inner_inset = scanline_inset(inner_y_local, inner.height, inner_radius);
        const float inner_left = inner.x + inner_inset;
        const float inner_right = inner.x + inner.width - inner_inset;

        if (inner_left > outer_left) {
            context.fill_rect(Rect{outer_left, y_abs, inner_left - outer_left, 1.0f}, color);
        }
        if (outer_right > inner_right) {
            context.fill_rect(Rect{inner_right, y_abs, outer_right - inner_right, 1.0f}, color);
        }
    }
}

inline void draw_outline(IGraphicsContext& context, const Rect& rect, const Color& color, float thickness) {
    Rect top{rect.x, rect.y, rect.width, thickness};
    Rect bottom{rect.x, rect.y + rect.height - thickness, rect.width, thickness};
    Rect left{rect.x, rect.y, thickness, rect.height};
    Rect right{rect.x + rect.width - thickness, rect.y, thickness, rect.height};
    context.fill_rect(top, color);
    context.fill_rect(bottom, color);
    context.fill_rect(left, color);
    context.fill_rect(right, color);
}

inline void draw_placeholder_box(IGraphicsContext& context, const Rect& rect, const Color& fill_color,
                                 const Color& stroke_color, bool fill_enabled = true, float stroke_thickness = 1.0f) {
    if (fill_enabled) {
        context.fill_rect(rect, fill_color);
    }
    draw_outline(context, rect, stroke_color, stroke_thickness);
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

inline void draw_box_decoration(IGraphicsContext& context, const Rect& rect, const Css::ComputedStyle* style,
                                const ImageBitmap* background_image) {
    if (!style) {
        return;
    }
    const ResolvedCorners corners = resolve_corners(style->border_radius, rect.width, rect.height);
    const float radius = std::max({corners.top_left, corners.top_right, corners.bottom_right, corners.bottom_left});
    if (style->box_shadow.has_value()) {
        const auto& shadow = *style->box_shadow;
        const float blur = std::max(0.0f, shadow.blur);
        Rect shadow_rect{
            rect.x + shadow.offset_x - blur,
            rect.y + shadow.offset_y - blur,
            rect.width + blur * 2.0f,
            rect.height + blur * 2.0f,
        };
        if (shadow_rect.width > 0.0f && shadow_rect.height > 0.0f) {
            Color shadow_color = shadow.color;
            if (blur > 0.0f) {
                shadow_color.a = static_cast<uint8_t>(shadow_color.a * 0.6f);
            }
            const float shadow_radius = std::max(0.0f, radius + blur);
            if (shadow_radius > 0.0f) {
                draw_rounded_fill(context, shadow_rect, shadow_radius, shadow_color);
            } else {
                context.fill_rect(shadow_rect, shadow_color);
            }
        }
    }
    if (style->background.has_value()) {
        if (corners.any()) {
            draw_rounded_fill_corners(context, rect, corners, *style->background);
        } else {
            context.fill_rect(rect, *style->background);
        }
    }
    if (background_image && style->background_image.has_value()) {
        draw_background_image(context, rect, *background_image, *style);
    }
    if (style->border_style != Css::ComputedStyle::BorderStyle::None) {
        const auto& bw = style->border_width;
        const auto& edge = style->border_edge_color;
        // A uniform-width border follows the corner radii (each corner rounds
        // independently). A single border color is used for the rounded stroke;
        // per-side colors only apply to the square multi-edge path below, which
        // is also the fallback when border widths differ per side.
        if (radius > 0.0f && is_uniform_border_width(bw)) {
            draw_rounded_border_corners(context, rect, corners, bw.top, style->border_color);
        } else {
            if (bw.top > 0.0f) {
                Rect top{rect.x, rect.y, rect.width, bw.top};
                context.fill_rect(top, edge.top);
            }
            if (bw.bottom > 0.0f) {
                Rect bottom{rect.x, rect.y + rect.height - bw.bottom, rect.width, bw.bottom};
                context.fill_rect(bottom, edge.bottom);
            }
            if (bw.left > 0.0f) {
                Rect left{rect.x, rect.y, bw.left, rect.height};
                context.fill_rect(left, edge.left);
            }
            if (bw.right > 0.0f) {
                Rect right{rect.x + rect.width - bw.right, rect.y, bw.right, rect.height};
                context.fill_rect(right, edge.right);
            }
        }
    }

    if (style->outline_width > 0.0f) {
        const float expand = style->outline_offset + style->outline_width;
        Rect outline_rect{rect.x - expand, rect.y - expand, rect.width + expand * 2.0f, rect.height + expand * 2.0f};
        if (outline_rect.width > 0.0f && outline_rect.height > 0.0f) {
            const float outline_radius = std::max(0.0f, radius + expand);
            if (outline_radius > 0.0f) {
                draw_uniform_rounded_border(context, outline_rect, outline_radius, style->outline_width,
                                            style->outline_color);
            } else {
                draw_outline(context, outline_rect, style->outline_color, style->outline_width);
            }
        }
    }
}

}  // namespace Hummingbird::Layout::PaintUtils
