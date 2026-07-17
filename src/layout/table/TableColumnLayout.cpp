#include "layout/table/TableColumnLayout.h"

#include <algorithm>
#include <numeric>
#include <optional>
#include <string_view>

#include "core/dom/Element.h"
#include "core/dom/ElementUtils.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "layout/RenderObject.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "layout/table/RenderTable.h"

namespace Hummingbird::Layout {

namespace {

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
        // No percentage resolution here yet (matches prior single-float behavior).
        return std::max(0.0f, style->width->has_percent ? style->width->percent : style->width->px);
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
            count += table_cell_colspan(*cell);
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
    if (style && style->width.has_value() && style->width->has_percent) {
        return std::max(0.0f, style->width->percent);
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

struct ColumnWidthHints {
    std::vector<float> percent;
    std::vector<float> absolute;
};

std::optional<float> cell_width_absolute_hint(const RenderTableCell& cell) {
    const auto* style = cell.get_computed_style();
    Metrics::Insets insets = Metrics::compute_insets(style);
    if (style && style->width.has_value() && !style->width->has_percent) {
        float width = std::max(0.0f, style->width->px);
        return Metrics::resolve_border_box_width(style, width, insets);
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
    if (!parsed || parsed->is_percent) {
        return std::nullopt;
    }
    float width = std::max(0.0f, parsed->value);
    return Metrics::resolve_border_box_width(style, width, insets);
}

ColumnWidthHints collect_column_hints(const std::vector<RenderTableRow*>& rows, size_t column_count) {
    ColumnWidthHints hints{std::vector<float>(column_count, 0.0f), std::vector<float>(column_count, 0.0f)};
    for (auto* row : rows) {
        size_t col = 0;
        for (const auto& child : row->get_children()) {
            auto* cell = dynamic_cast<RenderTableCell*>(child.get());
            if (!cell) {
                continue;
            }
            size_t span = table_cell_colspan(*cell);
            if (span == 0) {
                span = 1;
            }
            if (span == 1 && col < hints.percent.size()) {
                if (auto hint = cell_width_percent_hint(*cell)) {
                    hints.percent[col] = std::max(hints.percent[col], *hint);
                }
                if (auto hint = cell_width_absolute_hint(*cell)) {
                    hints.absolute[col] = std::max(hints.absolute[col], *hint);
                }
            }
            col += span;
        }
    }
    return hints;
}

void apply_column_absolute_min_widths(std::vector<float>& column_widths,
                                      const std::vector<float>& column_absolute_hints) {
    if (column_widths.size() != column_absolute_hints.size()) {
        return;
    }
    for (size_t i = 0; i < column_widths.size(); ++i) {
        float hint = column_absolute_hints[i];
        if (hint <= 0.0f) {
            continue;
        }
        column_widths[i] = std::max(column_widths[i], hint);
    }
}

void apply_column_percent_min_widths(std::vector<float>& column_widths, const std::vector<float>& column_percent_hints,
                                     float target_width) {
    if (target_width <= 0.0f || column_widths.size() != column_percent_hints.size()) {
        return;
    }
    float hinted_total = 0.0f;
    for (float hint : column_percent_hints) {
        if (hint > 0.0f) {
            hinted_total += hint;
        }
    }
    float normalization = hinted_total > 100.0f ? (100.0f / hinted_total) : 1.0f;
    for (size_t i = 0; i < column_widths.size(); ++i) {
        float hint = column_percent_hints[i];
        if (hint <= 0.0f) {
            continue;
        }
        float min_width = target_width * ((hint * normalization) / 100.0f);
        column_widths[i] = std::max(column_widths[i], min_width);
    }
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
            size_t span = table_cell_colspan(*cell);
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

}  // namespace

size_t table_cell_colspan(const RenderTableCell& cell) {
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

TableColumnLayoutResult compute_table_column_layout(RenderTable& table, IGraphicsContext& context,
                                                    float available_width) {
    const auto* style = table.get_computed_style();
    auto* element = static_cast<const DOM::Element*>(table.get_dom_node());

    std::vector<RenderTableRow*> rows;
    collect_rows(table, rows);
    size_t column_count = compute_column_count(rows);
    auto column_widths = compute_column_widths(context, rows, column_count);
    auto column_hints = collect_column_hints(rows, column_count);
    float target_width = resolve_table_target_width(*element, style, available_width);
    apply_column_absolute_min_widths(column_widths, column_hints.absolute);
    apply_column_percent_min_widths(column_widths, column_hints.percent, target_width);
    float content_width = sum_widths(column_widths);
    content_width = apply_target_width(column_widths, column_hints.percent, content_width, target_width);
    return {std::move(column_widths), content_width};
}

}  // namespace Hummingbird::Layout
