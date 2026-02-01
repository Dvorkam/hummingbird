#include "layout/PositioningUtils.h"

#include "layout/RenderObject.h"

namespace Hummingbird::Layout::Positioning {

namespace {
struct ContainingBlock {
    Rect rect;
    bool valid = false;
};

float resolve_horizontal_offset(const Css::ComputedStyle* style, float container_width, float box_width) {
    if (style && style->left.has_value()) {
        return *style->left;
    }
    if (style && style->right.has_value()) {
        return container_width - *style->right - box_width;
    }
    return 0.0f;
}

float resolve_vertical_offset(const Css::ComputedStyle* style, float container_height, float box_height) {
    if (style && style->top.has_value()) {
        return *style->top;
    }
    if (style && style->bottom.has_value()) {
        return container_height - *style->bottom - box_height;
    }
    return 0.0f;
}

void apply_positioning_recursive(RenderObject& node, IGraphicsContext& context, const Rect& viewport,
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
            dx += *style->left;
        } else if (style->right.has_value()) {
            dx -= *style->right;
        }
        if (style->top.has_value()) {
            dy += *style->top;
        } else if (style->bottom.has_value()) {
            dy -= *style->bottom;
        }
        rect.x += dx;
        rect.y += dy;
        node.set_rect(rect);
        abs = {parent_abs.x + rect.x, parent_abs.y + rect.y, rect.width, rect.height};
    }

    if (is_positioned(style)) {
        next_containing = {abs, true};
    }

    for (const auto& child : node.get_children()) {
        apply_positioning_recursive(*child, context, viewport, abs, next_containing);
    }
}
}  // namespace

void apply_positioning(RenderObject& root, IGraphicsContext& context, const Rect& viewport) {
    ContainingBlock containing{};
    Rect parent_abs{0.0f, 0.0f, 0.0f, 0.0f};
    apply_positioning_recursive(root, context, viewport, parent_abs, containing);
}

}  // namespace Hummingbird::Layout::Positioning
