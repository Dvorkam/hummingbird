#include "layout/table/RenderTable.h"

#include <stddef.h>

#include <algorithm>
#include <numeric>
#include <optional>
#include <string_view>

#include "core/dom/Element.h"
#include "core/dom/ElementUtils.h"
#include "core/dom/Node.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "layout/RenderObject.h"
#include "layout/geometry/Geometry.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Layout {

namespace {
constexpr float kTableMeasureWidth = 100000.0f;
constexpr float kTableGridStroke = 1.0f;
constexpr Color kTableGridColor{150, 150, 150, 255};

struct ParsedWidth {
    float value;
    bool is_percent;
};

std::optional<size_t> parse_span_value(std::string_view value) {
    auto parsed = Core::Utils::parse_long(value, Core::Utils::NumberParseMode::AllowTrailing);
    if (!parsed) {
        return std::nullopt;
    }
    if (*parsed < 1) {
        return 1U;
    }
    return static_cast<size_t>(*parsed);
}

size_t cell_colspan(const RenderTableCell& cell) {
    auto* element = dynamic_cast<const DOM::Element*>(cell.get_dom_node());
    if (!element) {
        return 1;
    }
    auto attr = DOM::find_attribute_value(*element, Hummingbird::Html::AttributeNames::ColSpan);
    if (!attr) {
        return 1;
    }
    auto parsed = parse_span_value(*attr);
    return parsed.value_or(1);
}

std::optional<ParsedWidth> parse_width_value(std::string_view value) {
    std::string_view trimmed = Core::Utils::trim_ascii_whitespace(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    bool is_percent = false;
    if (trimmed.back() == '%') {
        is_percent = true;
        trimmed.remove_suffix(1);
        trimmed = Core::Utils::trim_ascii_whitespace(trimmed);
    }

    auto parsed = Core::Utils::parse_float(trimmed, Core::Utils::NumberParseMode::AllowTrailing);
    if (!parsed) {
        return std::nullopt;
    }
    if (*parsed < 0.0f) {
        *parsed = 0.0f;
    }
    return ParsedWidth{*parsed, is_percent};
}

float resolve_table_target_width(const DOM::Element& element, const Css::ComputedStyle* style, float available_width) {
    if (style && style->width.has_value()) {
        return std::max(0.0f, *style->width);
    }
    auto attr = DOM::find_attribute_value(element, Hummingbird::Html::AttributeNames::Width);
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
        if (auto* cell = dynamic_cast<const RenderTableCell*>(child.get())) {
            count += cell_colspan(*cell);
        }
    }
    return count;
}

void collect_rows(const RenderObject& node, std::vector<RenderTableRow*>& rows) {
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

std::optional<float> cell_width_percent_hint(const RenderTableCell& cell) {
    const auto* style = cell.get_computed_style();
    if (style && style->width.has_value() && style->width_is_percent) {
        return std::max(0.0f, *style->width);
    }

    auto* element = dynamic_cast<const DOM::Element*>(cell.get_dom_node());
    if (!element) {
        return std::nullopt;
    }
    auto attr = DOM::find_attribute_value(*element, Hummingbird::Html::AttributeNames::Width);
    if (!attr) {
        return std::nullopt;
    }
    auto parsed = parse_width_value(*attr);
    if (!parsed || !parsed->is_percent) {
        return std::nullopt;
    }
    return parsed->value;
}

std::vector<float> collect_column_percent_hints(const std::vector<RenderTableRow*>& rows, size_t column_count) {
    std::vector<float> hints(column_count, 0.0f);
    for (auto* row : rows) {
        size_t col = 0;
        for (const auto& child : row->get_children()) {
            auto* cell = dynamic_cast<RenderTableCell*>(child.get());
            if (!cell) {
                continue;
            }
            size_t span = cell_colspan(*cell);
            if (span == 0) {
                span = 1;
            }
            if (span == 1 && col < hints.size()) {
                if (auto hint = cell_width_percent_hint(*cell)) {
                    hints[col] = std::max(hints[col], *hint);
                }
            }
            col += span;
        }
    }
    return hints;
}

void apply_column_percent_min_widths(std::vector<float>& column_widths, const std::vector<float>& column_percent_hints,
                                     float target_width) {
    if (target_width <= 0.0f || column_widths.size() != column_percent_hints.size()) {
        return;
    }
    for (size_t i = 0; i < column_widths.size(); ++i) {
        float hint = column_percent_hints[i];
        if (hint <= 0.0f) {
            continue;
        }
        float min_width = target_width * (hint / 100.0f);
        column_widths[i] = std::max(column_widths[i], min_width);
    }
}

std::vector<RenderTableRow*> collect_table_rows(RenderTable& table) {
    std::vector<RenderTableRow*> rows;
    collect_rows(table, rows);
    return rows;
}

size_t compute_column_count(const std::vector<RenderTableRow*>& rows) {
    size_t column_count = 0;
    for (const auto* row : rows) {
        column_count = std::max(column_count, count_cells(*row));
    }
    return column_count;
}

void distribute_cell_width(std::vector<float>& column_widths, size_t col, size_t span, float width) {
    if (column_widths.empty() || col >= column_widths.size() || span == 0) {
        return;
    }
    size_t limit = std::min(column_widths.size(), col + span);
    float current_width = 0.0f;
    for (size_t i = col; i < limit; ++i) {
        current_width += column_widths[i];
    }
    if (width <= current_width) {
        return;
    }
    float extra = width - current_width;
    float per_column = extra / static_cast<float>(limit - col);
    for (size_t i = col; i < limit; ++i) {
        column_widths[i] += per_column;
    }
}

std::vector<float> compute_column_widths(IGraphicsContext& context, const std::vector<RenderTableRow*>& rows,
                                         size_t column_count) {
    std::vector<float> column_widths(column_count, 0.0f);
    for (auto* row : rows) {
        size_t col = 0;
        for (const auto& child : row->get_children()) {
            auto* cell = dynamic_cast<RenderTableCell*>(child.get());
            if (!cell) {
                continue;
            }
            size_t span = cell_colspan(*cell);
            if (span == 0) {
                span = 1;
            }
            float width = cell->measure_intrinsic_width(context);
            distribute_cell_width(column_widths, col, span, width);
            col += span;
        }
    }
    return column_widths;
}

float apply_target_width(std::vector<float>& column_widths, const std::vector<float>& column_percent_hints,
                         float content_width, float target_width) {
    if (target_width <= 0.0f || target_width <= content_width) {
        return content_width;
    }
    if (!column_widths.empty()) {
        std::vector<float> weights(column_widths.size(), 1.0f);
        if (column_percent_hints.size() == column_widths.size()) {
            float hinted_total = 0.0f;
            size_t unhinted_count = 0;
            for (float hint : column_percent_hints) {
                if (hint > 0.0f) {
                    hinted_total += hint;
                } else {
                    ++unhinted_count;
                }
            }
            if (hinted_total > 0.0f) {
                float remainder = std::max(0.0f, 100.0f - hinted_total);
                float unhinted_weight = unhinted_count > 0 ? (remainder / static_cast<float>(unhinted_count)) : 0.0f;
                for (size_t i = 0; i < weights.size(); ++i) {
                    weights[i] = column_percent_hints[i] > 0.0f ? column_percent_hints[i] : unhinted_weight;
                }
            }
        }
        float total_weight = sum_widths(weights);
        if (total_weight <= 0.0f) {
            std::fill(weights.begin(), weights.end(), 1.0f);
            total_weight = static_cast<float>(weights.size());
        }
        float extra = target_width - content_width;
        for (size_t i = 0; i < column_widths.size(); ++i) {
            column_widths[i] += extra * (weights[i] / total_weight);
        }
    }
    return target_width;
}

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
    auto rows = collect_table_rows(*this);
    size_t column_count = compute_column_count(rows);
    auto column_widths = compute_column_widths(context, rows, column_count);
    auto column_percent_hints = collect_column_percent_hints(rows, column_count);
    auto* element = static_cast<const DOM::Element*>(get_dom_node());
    float target_width = resolve_table_target_width(*element, style, available_width);
    apply_column_percent_min_widths(column_widths, column_percent_hints, target_width);
    float content_width = sum_widths(column_widths);
    content_width = apply_target_width(column_widths, column_percent_hints, content_width, target_width);

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = insets.left + content_width + insets.right;
    m_rect.height = layout_table_children(*this, context, insets, content_width, column_widths);
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
        size_t span = cell_colspan(*cell);
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
        cursor_x += cell_width;
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

    Rect abs{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    if (abs.width <= 0.0f || abs.height <= 0.0f) {
        return;
    }

    context.fill_rect({abs.x, abs.y, abs.width, kTableGridStroke}, kTableGridColor);
    context.fill_rect({abs.x, abs.y + abs.height - kTableGridStroke, abs.width, kTableGridStroke}, kTableGridColor);
    context.fill_rect({abs.x, abs.y, kTableGridStroke, abs.height}, kTableGridColor);
    context.fill_rect({abs.x + abs.width - kTableGridStroke, abs.y, kTableGridStroke, abs.height}, kTableGridColor);
}

}  // namespace Hummingbird::Layout
