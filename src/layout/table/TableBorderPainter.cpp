#include "layout/table/TableBorderPainter.h"

#include <cmath>
#include <string>

#include "core/dom/Element.h"
#include "core/dom/ElementUtils.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/Log.h"
#include "html/HtmlAttributeNames.h"
#include "layout/geometry/Geometry.h"
#include "layout/table/RenderTable.h"
#include "layout/table/TableDebug.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kTableGridStroke = 1.0f;
constexpr Color kTableGridColor{150, 150, 150, 255};
}  // namespace

void paint_table_cell_fallback_border(const RenderTableCell& cell, IGraphicsContext& context, const Point& offset) {
    const LegacyBorderInfo legacy = inspect_legacy_border_info(cell.get_dom_node());
    const auto* style = cell.get_computed_style();

    if (!legacy.allows_fallback_grid) {
        log_table_grid_decision(cell, style, legacy, legacy.fallback_reason);
        return;
    }

    const bool seam_debug = should_log_table_seam(cell);
    const bool seam_verbose = seam_debug && table_seam_verbose_enabled();
    const auto* element = dynamic_cast<const DOM::Element*>(cell.get_dom_node());
    std::string class_name;
    std::string table_class = table_class_for_cell(cell);
    if (element) {
        if (auto cls = DOM::find_attribute_value(*element, Hummingbird::Html::AttributeNames::Class)) {
            class_name.assign(*cls);
        }
    }
    if (style && style->border_style != Css::ComputedStyle::BorderStyle::None) {
        const auto& bw = style->border_width;
        if (bw.top > 0.0f || bw.right > 0.0f || bw.bottom > 0.0f || bw.left > 0.0f) {
            log_table_grid_decision(cell, style, legacy, "skip_css_border");
            if (seam_verbose) {
                const Rect& rect = cell.get_rect();
                HB_LOG_WARN("[table-debug] skip fallback grid due css border table_class='"
                            << table_class << "' class='" << class_name << "' rect=(" << rect.x << "," << rect.y << ","
                            << rect.width << "," << rect.height << ") border=(" << bw.top << "," << bw.right << ","
                            << bw.bottom << "," << bw.left << ")");
            }
            return;
        }
    }

    const Rect& rect = cell.get_rect();
    Rect abs{offset.x + rect.x, offset.y + rect.y, rect.width, rect.height};
    if (abs.width <= 0.0f || abs.height <= 0.0f) {
        return;
    }

    float left = std::floor(abs.x + 0.001f);
    float top = std::floor(abs.y + 0.001f);
    float right = std::floor(abs.x + abs.width + 0.001f);
    float bottom = std::floor(abs.y + abs.height + 0.001f);
    float snapped_width = std::max(kTableGridStroke, right - left);
    float snapped_height = std::max(kTableGridStroke, bottom - top);
    log_table_grid_decision(cell, style, legacy, "draw_fallback_grid");

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
