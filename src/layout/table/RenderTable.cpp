#include "layout/table/RenderTable.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>

#include "core/dom/Element.h"
#include "core/dom/ElementUtils.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "core/utils/WarnOnce.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
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

struct ColumnWidthHints {
    std::vector<float> percent;
    std::vector<float> absolute;
};

std::optional<float> cell_width_absolute_hint(const RenderTableCell& cell) {
    const auto* style = cell.get_computed_style();
    Metrics::Insets insets = Metrics::compute_insets(style);
    if (style && style->width.has_value() && !style->width_is_percent) {
        float width = std::max(0.0f, *style->width);
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
            size_t span = cell_colspan(*cell);
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
    // Real pages sometimes overcommit percent hints (for example 70% + 70%).
    // Normalize to 100% so hints stay useful without forcing table overflow.
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

bool table_seam_debug_enabled() {
    static const bool enabled = std::getenv("HB_DEBUG_TABLE_SEAMS") != nullptr;
    return enabled;
}

bool table_seam_verbose_enabled() {
    static const bool enabled = std::getenv("HB_DEBUG_TABLE_SEAMS_VERBOSE") != nullptr;
    return enabled;
}

bool table_grid_debug_enabled() {
    static const bool enabled = std::getenv("HB_DEBUG_TABLE_GRID") != nullptr;
    return enabled;
}

std::string table_class_for_cell(const RenderTableCell& cell) {
    const RenderObject* node = &cell;
    while (node) {
        const auto* element = dynamic_cast<const DOM::Element*>(node->get_dom_node());
        if (element && element->get_tag_name() == Hummingbird::Html::TagNames::Table) {
            if (auto cls = DOM::find_attribute_value(*element, Hummingbird::Html::AttributeNames::Class)) {
                return std::string(*cls);
            }
            return {};
        }
        node = node->get_parent();
    }
    return {};
}

struct LegacyBorderInfo {
    bool found = false;
    std::string raw;
    std::optional<float> parsed;
    bool allows_fallback_grid = false;
    std::string fallback_reason;
    bool table_has_th = false;
    std::string table_border;
    std::string table_cellspacing;
    std::string table_cellpadding;
};

const DOM::Element* ancestor_table_element(const DOM::Node* node) {
    const auto* current = node;
    while (current) {
        const auto* element = dynamic_cast<const DOM::Element*>(current);
        if (element && element->get_tag_name() == Hummingbird::Html::TagNames::Table) {
            return element;
        }
        current = current->get_parent();
    }
    return nullptr;
}

bool element_subtree_contains_tag(const DOM::Node* node, std::string_view tag) {
    if (!node) {
        return false;
    }
    if (const auto* element = dynamic_cast<const DOM::Element*>(node)) {
        if (element->get_tag_name() == tag) {
            return true;
        }
        for (const auto& child : element->get_children()) {
            if (element_subtree_contains_tag(child.get(), tag)) {
                return true;
            }
        }
    }
    return false;
}

LegacyBorderInfo inspect_legacy_border_info(const DOM::Node* node) {
    LegacyBorderInfo info;

    const auto* table = ancestor_table_element(node);
    if (!table) {
        info.allows_fallback_grid = true;
        info.fallback_reason = "allow_no_table_ancestor";
        return info;
    }

    if (auto border = DOM::find_attribute_value(*table, Hummingbird::Html::AttributeNames::Border)) {
        info.found = true;
        info.raw = std::string(*border);
        info.table_border = info.raw;
        info.parsed = Core::Utils::parse_float(*border, Core::Utils::NumberParseMode::AllowTrailing);
    }
    if (auto spacing = DOM::find_attribute_value(*table, Hummingbird::Html::AttributeNames::CellSpacing)) {
        info.table_cellspacing = std::string(*spacing);
    }
    if (auto padding = DOM::find_attribute_value(*table, Hummingbird::Html::AttributeNames::CellPadding)) {
        info.table_cellpadding = std::string(*padding);
    }

    info.table_has_th = element_subtree_contains_tag(table, Hummingbird::Html::TagNames::Th);
    if (info.found) {
        if (info.parsed && *info.parsed <= 0.0f) {
            info.allows_fallback_grid = false;
            info.fallback_reason = "skip_legacy_border_zero";
        } else {
            info.allows_fallback_grid = true;
            info.fallback_reason = "allow_legacy_border_positive_or_unparsed";
        }
        return info;
    }

    if (info.table_has_th) {
        info.allows_fallback_grid = true;
        info.fallback_reason = "allow_header_table";
    } else {
        info.allows_fallback_grid = false;
        info.fallback_reason = "skip_likely_layout_table";
    }
    return info;
}

void log_table_grid_decision(const RenderTableCell& cell, const Css::ComputedStyle* style,
                             const LegacyBorderInfo& legacy, std::string_view decision) {
    if (!table_grid_debug_enabled()) {
        return;
    }
    const auto* element = dynamic_cast<const DOM::Element*>(cell.get_dom_node());
    const std::string table_class = table_class_for_cell(cell);
    std::string cell_class;
    if (element) {
        if (auto cls = DOM::find_attribute_value(*element, Hummingbird::Html::AttributeNames::Class)) {
            cell_class = std::string(*cls);
        }
    }

    float bw_top = style ? style->border_width.top : 0.0f;
    float bw_right = style ? style->border_width.right : 0.0f;
    float bw_bottom = style ? style->border_width.bottom : 0.0f;
    float bw_left = style ? style->border_width.left : 0.0f;
    auto border_style = style ? static_cast<int>(style->border_style) : -1;

    static Core::Utils::WarnOnce warn_once;
    std::string key = std::string(decision) + "|" + table_class + "|" + cell_class + "|" + legacy.raw + "|" +
                      legacy.table_border + "|" + legacy.table_cellspacing + "|" + legacy.table_cellpadding + "|" +
                      std::to_string(border_style) + "|" + std::to_string(static_cast<int>(bw_top)) + "|" +
                      std::to_string(static_cast<int>(bw_right)) + "|" + std::to_string(static_cast<int>(bw_bottom)) +
                      "|" + std::to_string(static_cast<int>(bw_left));
    if (!warn_once.should_log(key)) {
        return;
    }

    HB_LOG_WARN("[table-grid-debug] decision="
                << decision << " table_class='" << table_class << "' cell_class='" << cell_class
                << "' legacy_border_found=" << legacy.found << " legacy_border_raw='" << legacy.raw
                << "' legacy_border_parsed=" << (legacy.parsed ? std::to_string(*legacy.parsed) : "n/a")
                << " fallback_reason='" << legacy.fallback_reason << "' table_has_th=" << legacy.table_has_th
                << " table_border='" << legacy.table_border << "' table_cellspacing='" << legacy.table_cellspacing
                << "' table_cellpadding='" << legacy.table_cellpadding << "' border_style=" << border_style
                << " border_widths=(" << bw_top << "," << bw_right << "," << bw_bottom << "," << bw_left << ")");
}

bool should_log_table_seam(const RenderTableCell& cell) {
    if (!table_seam_debug_enabled()) {
        return false;
    }
    const std::string table_class = table_class_for_cell(cell);
    const char* filter_env = std::getenv("HB_DEBUG_TABLE_SEAMS_FILTER");
    const std::string filter = filter_env && *filter_env ? std::string(filter_env) : "table-balance-demo";
    if (filter.empty()) {
        return true;
    }
    return table_class.find(filter) != std::string::npos;
}

const RenderTableCell* previous_cell_in_row(const RenderTableCell& cell) {
    const auto* row = dynamic_cast<const RenderTableRow*>(cell.get_parent());
    if (!row) {
        return nullptr;
    }
    const RenderTableCell* previous = nullptr;
    for (const auto& child : row->get_children()) {
        const auto* current = dynamic_cast<const RenderTableCell*>(child.get());
        if (!current) {
            continue;
        }
        if (current == &cell) {
            return previous;
        }
        previous = current;
    }
    return nullptr;
}

void log_table_seam_anomaly(const RenderTableCell& cell) {
    if (!should_log_table_seam(cell)) {
        return;
    }
    const auto* previous = previous_cell_in_row(cell);
    if (!previous) {
        return;
    }
    float previous_right = previous->get_rect().x + previous->get_rect().width;
    float delta = cell.get_rect().x - previous_right;
    if (std::fabs(delta) <= 0.25f) {
        return;
    }
    static Core::Utils::WarnOnce warn_once;
    const int right_px = static_cast<int>(std::lround(previous_right * 10.0f));
    const int this_px = static_cast<int>(std::lround(cell.get_rect().x * 10.0f));
    std::string key = table_class_for_cell(cell) + ":" + std::to_string(right_px) + ":" + std::to_string(this_px);
    if (!warn_once.should_log(key)) {
        return;
    }
    HB_LOG_WARN("[table-debug] seam " << (delta < 0.0f ? "overlap" : "gap") << " table_class='"
                                      << table_class_for_cell(cell) << "' prev_right=" << previous_right
                                      << " current_left=" << cell.get_rect().x << " delta=" << delta);
}
}  // namespace

void RenderTable::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    Metrics::Insets insets = Metrics::compute_insets(style);
    float available_width = Metrics::compute_available_width(bounds, insets);
    auto rows = collect_table_rows(*this);
    size_t column_count = compute_column_count(rows);
    auto column_widths = compute_column_widths(context, rows, column_count);
    auto column_hints = collect_column_hints(rows, column_count);
    auto* element = static_cast<const DOM::Element*>(get_dom_node());
    float target_width = resolve_table_target_width(*element, style, available_width);
    apply_column_absolute_min_widths(column_widths, column_hints.absolute);
    apply_column_percent_min_widths(column_widths, column_hints.percent, target_width);
    float content_width = sum_widths(column_widths);
    content_width = apply_target_width(column_widths, column_hints.percent, content_width, target_width);

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
    log_table_seam_anomaly(*this);

    const LegacyBorderInfo legacy = inspect_legacy_border_info(get_dom_node());
    const auto* style = get_computed_style();

    if (!legacy.allows_fallback_grid) {
        log_table_grid_decision(*this, style, legacy, legacy.fallback_reason);
        return;
    }

    const bool seam_debug = should_log_table_seam(*this);
    const bool seam_verbose = seam_debug && table_seam_verbose_enabled();
    const auto* element = dynamic_cast<const DOM::Element*>(get_dom_node());
    std::string class_name;
    std::string table_class = table_class_for_cell(*this);
    if (element) {
        if (auto cls = DOM::find_attribute_value(*element, Hummingbird::Html::AttributeNames::Class)) {
            class_name.assign(*cls);
        }
    }
    if (style && style->border_style != Css::ComputedStyle::BorderStyle::None) {
        const auto& bw = style->border_width;
        if (bw.top > 0.0f || bw.right > 0.0f || bw.bottom > 0.0f || bw.left > 0.0f) {
            log_table_grid_decision(*this, style, legacy, "skip_css_border");
            if (seam_verbose) {
                HB_LOG_WARN("[table-debug] skip fallback grid due css border table_class='"
                            << table_class << "' class='" << class_name << "' rect=(" << m_rect.x << "," << m_rect.y
                            << "," << m_rect.width << "," << m_rect.height << ") border=(" << bw.top << "," << bw.right
                            << "," << bw.bottom << "," << bw.left << ")");
            }
            return;
        }
    }

    Rect abs{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    if (abs.width <= 0.0f || abs.height <= 0.0f) {
        return;
    }

    // Snap all edges with the same rule to keep adjacent cell seams on one pixel.
    float left = std::floor(abs.x + 0.001f);
    float top = std::floor(abs.y + 0.001f);
    float right = std::floor(abs.x + abs.width + 0.001f);
    float bottom = std::floor(abs.y + abs.height + 0.001f);
    float snapped_width = std::max(kTableGridStroke, right - left);
    float snapped_height = std::max(kTableGridStroke, bottom - top);
    log_table_grid_decision(*this, style, legacy, "draw_fallback_grid");

    if (seam_verbose) {
        HB_LOG_WARN("[table-debug] draw fallback grid table_class='"
                    << table_class << "' class='" << class_name << "' abs=(" << abs.x << "," << abs.y << ","
                    << abs.width << "," << abs.height << ") snapped=(" << left << "," << top << "," << right << ","
                    << bottom << ")");
    }

    context.fill_rect({left, top, snapped_width, kTableGridStroke}, kTableGridColor);
    context.fill_rect({left, bottom, snapped_width, kTableGridStroke}, kTableGridColor);
    context.fill_rect({left, top, kTableGridStroke, snapped_height}, kTableGridColor);
    context.fill_rect({right, top, kTableGridStroke, snapped_height}, kTableGridColor);
}

}  // namespace Hummingbird::Layout
