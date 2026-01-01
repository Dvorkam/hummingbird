#include "layout/RenderTable.h"

#include <algorithm>
#include <numeric>

namespace Hummingbird::Layout {

namespace {
constexpr float kTableMeasureWidth = 100000.0f;

struct Insets {
    float left;
    float right;
    float top;
    float bottom;
};

Insets compute_insets(const Css::ComputedStyle* style) {
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float border_top = style ? style->border_width.top : 0.0f;
    float border_bottom = style ? style->border_width.bottom : 0.0f;
    return {padding_left + border_left, padding_right + border_right, padding_top + border_top,
            padding_bottom + border_bottom};
}

size_t count_cells(const RenderTableRow& row) {
    size_t count = 0;
    for (const auto& child : row.get_children()) {
        if (dynamic_cast<const RenderTableCell*>(child.get())) {
            ++count;
        }
    }
    return count;
}

void collect_rows(RenderObject& node, std::vector<RenderTableRow*>& rows) {
    for (const auto& child : node.get_children()) {
        if (auto* row = dynamic_cast<RenderTableRow*>(child.get())) {
            rows.push_back(row);
            continue;
        }
        if (auto* section = dynamic_cast<RenderTableSection*>(child.get())) {
            collect_rows(*section, rows);
        }
    }
}

float sum_widths(const std::vector<float>& widths) {
    return std::accumulate(widths.begin(), widths.end(), 0.0f);
}
}  // namespace

void RenderTable::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    Insets insets = compute_insets(style);

    std::vector<RenderTableRow*> rows;
    collect_rows(*this, rows);

    size_t column_count = 0;
    for (const auto* row : rows) {
        column_count = std::max(column_count, count_cells(*row));
    }

    std::vector<float> column_widths(column_count, 0.0f);
    for (auto* row : rows) {
        size_t col = 0;
        for (const auto& child : row->get_children()) {
            auto* cell = dynamic_cast<RenderTableCell*>(child.get());
            if (!cell) {
                continue;
            }
            float width = cell->measure_intrinsic_width(context);
            if (col < column_widths.size()) {
                column_widths[col] = std::max(column_widths[col], width);
            }
            ++col;
        }
    }

    float content_width = sum_widths(column_widths);

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = insets.left + content_width + insets.right;

    float cursor_y = insets.top;
    for (const auto& child : m_children) {
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

    m_rect.height = cursor_y + insets.bottom;
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
        float cell_width = col < column_widths.size() ? column_widths[col] : 0.0f;
        Rect cell_bounds{cursor_x, 0.0f, cell_width, 0.0f};
        cell->layout(context, cell_bounds);
        row_height = std::max(row_height, cell->get_rect().height);
        cursor_x += cell_width;
        ++col;
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
        float right = child->get_rect().x + child->get_rect().width + margin_right;
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

}  // namespace Hummingbird::Layout
