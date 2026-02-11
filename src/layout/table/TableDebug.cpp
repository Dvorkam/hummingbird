#include "layout/table/TableDebug.h"

#include <cmath>
#include <cstdlib>

#include "core/dom/Element.h"
#include "core/dom/ElementUtils.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/WarnOnce.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "layout/table/RenderTable.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

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

namespace {
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
}  // namespace

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

}  // namespace Hummingbird::Layout
