#include "layout/formatting/RenderListItem.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "core/dom/Element.h"
#include "core/platform_api/IGraphicsContext.h"
#include "html/HtmlTagNames.h"
#include "layout/block/FloatLayoutUtils.h"
#include "layout/flow/FlowLayoutUtils.h"
#include "layout/flow/TextStyleUtils.h"
#include "layout/flow/inline/InlineRef.h"
#include "layout/geometry/Geometry.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird {
namespace DOM {
class Node;
}  // namespace DOM
}  // namespace Hummingbird

namespace Hummingbird::Layout {

namespace {
struct MarkerLayout {
    bool show = false;
    bool text = false;
    std::string label;
    float width = 0.0f;
    float height = 0.0f;
};

std::optional<std::string> decimal_marker_label(const DOM::Node* node) {
    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (!element || element->get_tag_name() != Hummingbird::Html::TagNames::Li) {
        return std::nullopt;
    }
    const auto* parent = dynamic_cast<const DOM::Element*>(element->get_parent());
    if (!parent || parent->get_tag_name() != Hummingbird::Html::TagNames::Ol) {
        return std::nullopt;
    }
    int ordinal = 0;
    for (const auto& child : parent->get_children()) {
        const auto* child_element = dynamic_cast<const DOM::Element*>(child.get());
        if (!child_element || child_element->get_tag_name() != Hummingbird::Html::TagNames::Li) {
            continue;
        }
        ++ordinal;
        if (child.get() == node) {
            break;
        }
    }
    if (ordinal <= 0) {
        return std::nullopt;
    }
    return std::to_string(ordinal) + ".";
}

MarkerLayout compute_marker_layout(IGraphicsContext& context, const DOM::Node* node, const Css::ComputedStyle* style) {
    MarkerLayout marker;
    if (style && style->list_style_type == Css::ComputedStyle::ListStyleType::None) {
        return marker;
    }
    marker.show = true;
    if (!(style && style->list_style_type == Css::ComputedStyle::ListStyleType::Decimal)) {
        marker.width = kListMarkerSizePx;
        marker.height = kListMarkerSizePx;
        return marker;
    }
    auto label = decimal_marker_label(node);
    if (!label.has_value()) {
        marker.width = kListMarkerSizePx;
        marker.height = kListMarkerSizePx;
        return marker;
    }
    marker.text = true;
    marker.label = *label;
    TextStyle text_style = TextStyleUtils::build_text_style(style);
    TextMetrics metrics = context.measure_text(marker.label, text_style);
    marker.width = std::max(metrics.width, 1.0f);
    marker.height = std::max(metrics.height, style ? style->font_size : 16.0f);
    return marker;
}

void update_marker_for_block(bool& marker_y_set, float& marker_y, float inset_top) {
    if (marker_y_set) {
        return;
    }
    marker_y = inset_top;
    marker_y_set = true;
}

void update_marker_for_inline(const InlineLayout::InlineLayoutResult& inline_layout, bool& marker_y_set,
                              float& marker_y, float inset_top, float marker_height) {
    if (marker_y_set || inline_layout.heights.empty()) {
        return;
    }
    marker_y = inset_top + std::max(0.0f, (inline_layout.heights[0] - marker_height) * 0.5f);
    marker_y_set = true;
}
}  // namespace

RenderListItem::RenderListItem(const DOM::Node* dom_node) : BlockBox(dom_node) {
    m_marker = RenderMarker::create(dom_node);
}

const Rect& RenderListItem::marker_rect() const {
    return m_marker ? m_marker->get_rect() : m_rect;
}

void RenderListItem::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    MarkerLayout marker = compute_marker_layout(context, get_dom_node(), style);
    float marker_offset = marker.show ? (marker.width + kListMarkerGapPx) : 0.0f;
    Metrics::BoxMetrics metrics =
        Metrics::compute_box_metrics(style, bounds, m_rect, Metrics::BoxWidthPolicy::WidthOnly, marker_offset);
    FlowLayout::LineCursor cursor{metrics.insets.left + marker_offset, metrics.insets.top, 0.0f};
    float marker_y = metrics.insets.top;
    bool marker_y_set = false;
    std::vector<FloatLayout::FloatBox> floats;
    float max_float_bottom = cursor.y;

    size_t i = 0;
    while (i < m_children.size()) {
        auto& child = m_children[i];
        const auto* child_style = child->get_computed_style();
        FlowLayout::ChildMargins margins = FlowLayout::compute_child_margins(child_style, false);

        Css::ComputedStyle::Float float_type = FlowLayout::resolve_float_type(*child, false);
        if (float_type != Css::ComputedStyle::Float::None) {
            FlowLayout::layout_float_child(context, *child, margins, cursor, float_type, floats, max_float_bottom,
                                           metrics.insets.left + marker_offset,
                                           metrics.insets.left + marker_offset + metrics.content_width);
            update_marker_for_block(marker_y_set, marker_y, metrics.insets.top);
            ++i;
            continue;
        }

        if (!child->Inline()) {
            float content_left = metrics.insets.left + marker_offset;
            float content_width = metrics.content_width;
            if (!floats.empty()) {
                float line_height_hint = FloatLayout::kFloatLineHeightFallback;
                if (child_style) {
                    if (child_style->line_height > 0.0f) {
                        line_height_hint = child_style->line_height;
                    } else {
                        line_height_hint = child_style->font_size;
                    }
                }
                if (line_height_hint <= 0.0f) {
                    line_height_hint = FloatLayout::kFloatLineHeightFallback;
                }
                float margin_top = margins.top > 0.0f ? margins.top : 0.0f;
                float band_y = cursor.y + margin_top;
                FloatLayout::FloatBand band = FloatLayout::compute_float_band(
                    floats, band_y, line_height_hint, metrics.insets.left + marker_offset,
                    metrics.insets.left + marker_offset + metrics.content_width);
                if (band.has_overlap && (band.right - band.left) <= 0.0f && band.clear_y > band_y) {
                    cursor.y = band.clear_y;
                    band_y = cursor.y + margin_top;
                    band = FloatLayout::compute_float_band(floats, band_y, line_height_hint,
                                                           metrics.insets.left + marker_offset,
                                                           metrics.insets.left + marker_offset + metrics.content_width);
                }
                content_left = band.left;
                content_width = band.right - band.left;
            }
            FlowLayout::layout_block_child(context, *child, margins, cursor, content_left, content_width);
            update_marker_for_block(marker_y_set, marker_y, metrics.insets.top);
            ++i;
            continue;
        }

        auto align = style ? style->text_align : Css::ComputedStyle::TextAlign::Left;
        float line_height_hint =
            std::max(cursor.line_height, style ? style->font_size : FloatLayout::kFloatLineHeightFallback);
        if (line_height_hint <= 0.0f) {
            line_height_hint = FloatLayout::kFloatLineHeightFallback;
        }
        FloatLayout::FloatBand band =
            FloatLayout::compute_float_band(floats, cursor.y, line_height_hint, metrics.insets.left + marker_offset,
                                            metrics.insets.left + marker_offset + metrics.content_width);
        if (band.has_overlap && (band.right - band.left) <= 0.0f && band.clear_y > cursor.y) {
            cursor.y = band.clear_y;
            band =
                FloatLayout::compute_float_band(floats, cursor.y, line_height_hint, metrics.insets.left + marker_offset,
                                                metrics.insets.left + marker_offset + metrics.content_width);
        }
        float wrap_width =
            (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap) ? 0.0f : (band.right - band.left);
        bool no_wrap = style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap;
        bool text_overflow_ellipsis = style && style->text_overflow == Css::ComputedStyle::TextOverflow::Ellipsis;
        cursor.x = std::max(cursor.x, band.left);
        InlineLayout::InlineLayoutResult inline_layout =
            FlowLayout::layout_inline_group(context, m_children, i, cursor, band.left, band.right - band.left, align,
                                            wrap_width, no_wrap, text_overflow_ellipsis, true);
        update_marker_for_inline(inline_layout, marker_y_set, marker_y, metrics.insets.top, marker.height);
    }

    FlowLayout::flush_line(cursor, metrics.insets.left + marker_offset);
    float content_bottom = std::max(cursor.y, max_float_bottom);
    m_rect.height = content_bottom + metrics.insets.bottom;

    if (m_marker && marker.show) {
        Rect marker_bounds{metrics.insets.left, marker_y, marker.width, marker.height};
        if (style && style->list_style_position == Css::ComputedStyle::ListStylePosition::Inside) {
            marker_bounds.x = metrics.insets.left;
        }
        if (marker.text) {
            m_marker->set_text(std::move(marker.label), marker.width, marker.height);
        } else {
            m_marker->set_disc(kListMarkerSizePx);
        }
        m_marker->layout(context, marker_bounds);
    } else if (m_marker) {
        m_marker->set_rect({});
    }
}

void RenderListItem::paint_self(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    bool show_marker = !(style && style->list_style_type == Css::ComputedStyle::ListStyleType::None);
    if (m_marker && show_marker) {
        Point marker_offset{offset.x + m_rect.x, offset.y + m_rect.y};
        m_marker->paint(context, marker_offset);
    }
    RenderObject::paint_self(context, offset);
}

void RenderMarker::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    m_rect = bounds;
    if (m_kind == Kind::Disc) {
        m_rect.width = m_size;
        m_rect.height = m_size;
    } else {
        m_rect.width = m_text_width;
        m_rect.height = m_text_height;
    }
}

void RenderMarker::set_disc(float size) {
    m_kind = Kind::Disc;
    m_size = size;
    m_text.clear();
    m_text_width = 0.0f;
    m_text_height = 0.0f;
}

void RenderMarker::set_text(std::string text, float width, float height) {
    m_kind = Kind::Text;
    m_text = std::move(text);
    m_text_width = width;
    m_text_height = height;
}

void RenderMarker::paint_self(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    Color color = style ? style->color : Color{0, 0, 0, 255};
    Rect absolute{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    if (m_kind == Kind::Disc) {
        context.fill_rect(absolute, color);
        return;
    }
    if (m_text.empty()) {
        return;
    }
    TextStyle text_style = TextStyleUtils::build_text_style(style);
    text_style.color = color;
    context.draw_text(m_text, absolute.x, absolute.y, text_style);
}

}  // namespace Hummingbird::Layout
