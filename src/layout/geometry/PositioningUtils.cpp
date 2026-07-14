#include "layout/geometry/PositioningUtils.h"

#include <algorithm>

#include "layout/RenderObject.h"

namespace Hummingbird::Layout::Positioning {

namespace {
struct ContainingBlock {
    Rect rect;
    bool valid = false;
};

float resolve_inset(float value, bool is_percent, float reference) {
    return is_percent ? reference * (value / 100.0f) : value;
}

float resolve_horizontal_offset(const Css::ComputedStyle* style, float container_width, float box_width) {
    if (!style) {
        return 0.0f;
    }
    // Opposing insets + auto side margins center the box (the auto margins
    // absorb the leftover space equally): `left:0; right:0; margin:auto`.
    if (style->left.has_value() && style->right.has_value() && style->margin_left_auto && style->margin_right_auto) {
        float left = resolve_inset(*style->left, style->left_is_percent, container_width);
        float right = resolve_inset(*style->right, style->right_is_percent, container_width);
        float free = container_width - left - right - box_width;
        return left + std::max(0.0f, free) / 2.0f;
    }
    if (style->left.has_value()) {
        return resolve_inset(*style->left, style->left_is_percent, container_width);
    }
    if (style->right.has_value()) {
        return container_width - resolve_inset(*style->right, style->right_is_percent, container_width) - box_width;
    }
    return 0.0f;
}

float resolve_vertical_offset(const Css::ComputedStyle* style, float container_height, float box_height) {
    if (!style) {
        return 0.0f;
    }
    // `top:0; bottom:0; margin:auto` centers vertically (DDG search button).
    if (style->top.has_value() && style->bottom.has_value() && style->margin_top_auto && style->margin_bottom_auto) {
        float top = resolve_inset(*style->top, style->top_is_percent, container_height);
        float bottom = resolve_inset(*style->bottom, style->bottom_is_percent, container_height);
        float free = container_height - top - bottom - box_height;
        return top + std::max(0.0f, free) / 2.0f;
    }
    if (style->top.has_value()) {
        return resolve_inset(*style->top, style->top_is_percent, container_height);
    }
    if (style->bottom.has_value()) {
        return container_height - resolve_inset(*style->bottom, style->bottom_is_percent, container_height) -
               box_height;
    }
    return 0.0f;
}

bool apply_positioning_recursive(RenderObject& node, IGraphicsContext& context, const Rect& viewport,
                                 const Rect& parent_abs, const ContainingBlock& containing) {
    const auto* style = node.get_computed_style();
    Rect rect = node.get_rect();
    Rect abs{parent_abs.x + rect.x, parent_abs.y + rect.y, rect.width, rect.height};

    ContainingBlock next_containing = containing;

    if (is_absolute(style)) {
        Rect base = containing.valid ? containing.rect : viewport;
        Rect layout_bounds{0.0f, 0.0f, base.width, 0.0f};
        node.layout(context, layout_bounds);
        rect = node.get_rect();
        float x = base.x + resolve_horizontal_offset(style, base.width, rect.width);
        float y = base.y + resolve_vertical_offset(style, base.height, rect.height);
        rect.x = x - parent_abs.x;
        rect.y = y - parent_abs.y;
        node.set_rect(rect);
        abs = {x, y, rect.width, rect.height};
    } else if (style && style->position == Css::ComputedStyle::Position::Relative) {
        float dx = 0.0f;
        float dy = 0.0f;
        if (style->left.has_value()) {
            dx += style->left_is_percent ? parent_abs.width * (*style->left / 100.0f) : *style->left;
        } else if (style->right.has_value()) {
            float right = style->right_is_percent ? parent_abs.width * (*style->right / 100.0f) : *style->right;
            dx -= right;
        }
        if (style->top.has_value()) {
            dy += style->top_is_percent ? parent_abs.height * (*style->top / 100.0f) : *style->top;
        } else if (style->bottom.has_value()) {
            float bottom = style->bottom_is_percent ? parent_abs.height * (*style->bottom / 100.0f) : *style->bottom;
            dy -= bottom;
        }
        rect.x += dx;
        rect.y += dy;
        node.set_rect(rect);
        abs = {parent_abs.x + rect.x, parent_abs.y + rect.y, rect.width, rect.height};
    }

    if (is_positioned(style)) {
        next_containing = {abs, true};
    }

    bool has_absolute = is_absolute(style);
    for (const auto& child : node.get_children()) {
        if (apply_positioning_recursive(*child, context, viewport, abs, next_containing)) {
            has_absolute = true;
        }
    }
    node.set_has_absolute_descendant(has_absolute);
    return has_absolute;
}
}  // namespace

void apply_positioning(RenderObject& root, IGraphicsContext& context, const Rect& viewport) {
    ContainingBlock containing{};
    Rect parent_abs{0.0f, 0.0f, 0.0f, 0.0f};
    apply_positioning_recursive(root, context, viewport, parent_abs, containing);
}

}  // namespace Hummingbird::Layout::Positioning
