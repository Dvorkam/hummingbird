#include "layout/RenderTable.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>

#include "core/dom/Element.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kTableMeasureWidth = 100000.0f;

struct Insets {
    float left;
    float right;
    float top;
    float bottom;
};

struct ParsedWidth {
    float value;
    bool is_percent;
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

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string_view trim(std::string_view view) {
    while (!view.empty() && std::isspace(static_cast<unsigned char>(view.front()))) {
        view.remove_prefix(1);
    }
    while (!view.empty() && std::isspace(static_cast<unsigned char>(view.back()))) {
        view.remove_suffix(1);
    }
    return view;
}

std::optional<std::string_view> find_attribute_value(const DOM::Element& element, std::string_view name) {
    for (const auto& [key, value] : element.get_attributes()) {
        if (iequals(key, name)) {
            return std::string_view(value);
        }
    }
    return std::nullopt;
}

std::optional<ParsedWidth> parse_width_value(std::string_view value) {
    std::string_view trimmed = trim(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    bool is_percent = false;
    if (trimmed.back() == '%') {
        is_percent = true;
        trimmed.remove_suffix(1);
        trimmed = trim(trimmed);
        if (trimmed.empty()) {
            return std::nullopt;
        }
    }

    std::string temp(trimmed);
    char* end = nullptr;
    float parsed = std::strtof(temp.c_str(), &end);
    if (end == temp.c_str()) {
        return std::nullopt;
    }
    if (parsed < 0.0f) {
        parsed = 0.0f;
    }
    return ParsedWidth{parsed, is_percent};
}

float resolve_table_target_width(const DOM::Element& element, const Css::ComputedStyle* style,
                                 float available_width) {
    if (style && style->width.has_value()) {
        return std::max(0.0f, *style->width);
    }
    auto attr = find_attribute_value(element, "width");
    if (!attr) {
        return 0.0f;
    }
    auto parsed = parse_width_value(*attr);
    if (!parsed) {
        return 0.0f;
    }
    if (parsed->is_percent) {
        return std::max(0.0f, available_width * (parsed->value / 100.0f));
    }
    return parsed->value;
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
    float available_width = bounds.width - insets.left - insets.right;
    if (available_width < 0.0f) {
        available_width = 0.0f;
    }

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
    auto* element = static_cast<const DOM::Element*>(get_dom_node());
    float target_width = resolve_table_target_width(*element, style, available_width);
    if (target_width > content_width) {
        if (!column_widths.empty()) {
            float extra = target_width - content_width;
            float per_column = extra / static_cast<float>(column_widths.size());
            for (auto& width : column_widths) {
                width += per_column;
            }
        }
        content_width = target_width;
    } else if (target_width > 0.0f) {
        content_width = std::max(content_width, target_width);
    }

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
