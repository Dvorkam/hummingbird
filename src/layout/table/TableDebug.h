#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Hummingbird::Css {
struct ComputedStyle;
}

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Layout {
class RenderTableCell;

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

bool table_seam_debug_enabled();
bool table_seam_verbose_enabled();
bool table_grid_debug_enabled();
std::string table_class_for_cell(const RenderTableCell& cell);
LegacyBorderInfo inspect_legacy_border_info(const DOM::Node* node);
void log_table_grid_decision(const RenderTableCell& cell, const Css::ComputedStyle* style,
                             const LegacyBorderInfo& legacy, std::string_view decision);
bool should_log_table_seam(const RenderTableCell& cell);
void log_table_seam_anomaly(const RenderTableCell& cell);

}  // namespace Hummingbird::Layout
