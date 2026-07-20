#include "layout/table/RenderTable.h"

#include <algorithm>

#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "layout/table/TableBorderPainter.h"
#include "layout/table/TableColumnLayout.h"
#include "layout/table/TableDebug.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Layout {

namespace {
constexpr float kTableMeasureWidth = 100000.0f;

float layout_table_children(RenderTable& table, IGraphicsContext& context, const Metrics::Insets& insets,
                            float content_width, const std::vector<float>& column_widths) {
    float cursor_y = insets.top;
    for (const auto& child : table.get_children()) {
        if (auto* section = dynamic_cast<RenderTableSection*>(child.get())) {
            Rect section_bounds{insets.left, cursor_y, content_width, 0.0f};
            section->layout_rows(context, section_bounds, column_widths);
            cursor_y += section->get_rect().height;
            continue;
        }
        if (auto* row = dynamic_cast<RenderTableRow*>(child.get())) {
            Rect row_bounds{insets.left, cursor_y, content_width, 0.0f};
            row->layout_row(context, row_bounds, column_widths);
            cursor_y += row->get_rect().height;
        }
    }
    return cursor_y + insets.bottom;
}

}  // namespace

void RenderTable::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    Metrics::Insets insets = Metrics::compute_insets(style);
    float available_width = Metrics::compute_available_width(bounds, insets);
    auto plan = compute_table_column_layout(*this, context, available_width);

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = insets.left + plan.content_width + insets.right;
    m_rect.height = layout_table_children(*this, context, insets, plan.content_width, plan.column_widths);
}

void RenderTableSection::layout_rows(IGraphicsContext& context, const Rect& bounds,
                                     const std::vector<float>& column_widths) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = bounds.width;

    float cursor_y = 0.0f;
    for (const auto& child : m_children) {
        auto* row = dynamic_cast<RenderTableRow*>(child.get());
        if (!row) {
            continue;
        }
        Rect row_bounds{0.0f, cursor_y, bounds.width, 0.0f};
        row->layout_row(context, row_bounds, column_widths);
        cursor_y += row->get_rect().height;
    }

    m_rect.height = cursor_y;
}

void RenderTableRow::layout_row(IGraphicsContext& context, const Rect& bounds,
                                const std::vector<float>& column_widths) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;

    float cursor_x = 0.0f;
    float row_height = 0.0f;
    size_t col = 0;

    for (const auto& child : m_children) {
        auto* cell = dynamic_cast<RenderTableCell*>(child.get());
        if (!cell) {
            continue;
        }
        size_t span = table_cell_colspan(*cell);
        if (span == 0) {
            span = 1;
        }
        float cell_width = 0.0f;
        size_t limit = std::min(column_widths.size(), col + span);
        for (size_t i = col; i < limit; ++i) {
            cell_width += column_widths[i];
        }
        Rect cell_bounds{cursor_x, 0.0f, cell_width, 0.0f};
        cell->layout(context, cell_bounds);
        row_height = std::max(row_height, cell->get_rect().height);
        cursor_x += cell->get_rect().width;
        col += span;
    }

    m_rect.width = cursor_x;
    m_rect.height = row_height;
}

float RenderTableCell::measure_intrinsic_width(IGraphicsContext& context) {
    BlockBox::layout(context, {0.0f, 0.0f, kTableMeasureWidth, 0.0f});

    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float inset_left = padding_left + border_left;
    float inset_right = padding_right + border_right;

    float content_right = inset_left;
    for (const auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        // Use the child's content width, not its rendered width: a display:block
        // child stretched to fill the measurement box would otherwise report
        // ~100000px and balloon this column (T-LAYOUT-TABLE-INTRINSIC-BLOCK-1).
        float right = child->get_rect().x + Metrics::max_content_width(*child) + margin_right;
        content_right = std::max(content_right, right);
    }

    float required_width = content_right + inset_right;
    float minimum_width = inset_left + inset_right;
    if (required_width < minimum_width) {
        required_width = minimum_width;
    }

    m_rect.width = required_width;
    return required_width;
}

void RenderTableCell::layout(IGraphicsContext& context, const Rect& bounds) {
    BlockBox::layout(context, bounds);

    const auto* style = get_computed_style();
    if (!style || style->text_align == Css::ComputedStyle::TextAlign::Left) {
        return;
    }

    Metrics::Insets insets = Metrics::compute_insets(style);
    float content_width = m_rect.width - insets.left - insets.right;
    if (content_width <= 0.0f) {
        return;
    }

    for (const auto& child : m_children) {
        if (!child || child->Inline()) {
            continue;
        }
        const auto* child_style = child->get_computed_style();
        if (child_style && child_style->float_type != Css::ComputedStyle::Float::None) {
            continue;
        }
        if (child_style && (child_style->margin_left_auto || child_style->margin_right_auto)) {
            continue;
        }
        float margin_left = child_style ? child_style->margin.left : 0.0f;
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float margin_box_width = child->get_rect().width + margin_left + margin_right;
        float free_space = content_width - margin_box_width;
        if (free_space < 0.0f) {
            free_space = 0.0f;
        }

        float margin_box_x = insets.left;
        if (style->text_align == Css::ComputedStyle::TextAlign::Center) {
            margin_box_x += free_space * 0.5f;
        } else if (style->text_align == Css::ComputedStyle::TextAlign::Right) {
            margin_box_x += free_space;
        }

        float child_x = margin_box_x + margin_left;
        child->set_rect({child_x, child->get_rect().y, child->get_rect().width, child->get_rect().height});
    }
}

void RenderTableCell::paint_self(IGraphicsContext& context, const Point& offset) const {
    BlockBox::paint_self(context, offset);
    log_table_seam_anomaly(*this);
    paint_table_cell_fallback_border(*this, context, offset);
}

}  // namespace Hummingbird::Layout
