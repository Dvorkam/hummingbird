#include "layout/flex/FlexBox.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "core/utils/Log.h"
#include "layout/flow/FlowLayoutUtils.h"
#include "layout/geometry/PositioningUtils.h"
#include "layout/geometry/metrics/InlineBaselineUtils.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Layout {

namespace {

constexpr float kIntrinsicMeasureWidth = 100000.0f;

using Css::ComputedStyle;

struct FlexItem {
    RenderObject* box = nullptr;
    FlowLayout::ChildMargins margins;
    float base_main = 0.0f;        // hypothetical main size (border-box)
    float target_main = 0.0f;      // main size after grow/shrink
    float cross = 0.0f;            // natural cross size (border-box)
    float baseline_ascent = 0.0f;  // border-box top -> first text baseline (row/baseline align)
    float grow = 0.0f;
    float shrink = 1.0f;
    int order = 0;
    bool has_explicit_cross = false;
};

float main_margins(const FlexItem& item, bool row) {
    return row ? item.margins.left + item.margins.right : item.margins.top + item.margins.bottom;
}

float cross_margins(const FlexItem& item, bool row) {
    return row ? item.margins.top + item.margins.bottom : item.margins.left + item.margins.right;
}

bool is_inline_level(RenderObject& box) {
    return static_cast<bool>(box.Inline());
}

std::optional<float> resolve_definite_content_height(const ComputedStyle* style, const Rect& bounds,
                                                     const Metrics::Insets& insets) {
    if (!style || !style->height.has_value()) {
        return std::nullopt;
    }
    if (style->height->has_percent && bounds.height <= 0.0f) {
        return std::nullopt;
    }
    float resolved = Metrics::resolve_axis_length(*style->height, bounds.height);
    float border_box = Metrics::resolve_border_box_height(style, resolved, insets);
    return std::max(0.0f, border_box - insets.top - insets.bottom);
}

void force_rect_main(RenderObject& box, bool row, float target) {
    Rect rect = box.get_rect();
    if (row) {
        if (std::abs(rect.width - target) > 0.5f) {
            rect.width = target;
            box.set_rect(rect);
        }
    } else {
        if (std::abs(rect.height - target) > 0.5f) {
            rect.height = target;
            box.set_rect(rect);
        }
    }
}

void apply_explicit_height(RenderObject& box, const ComputedStyle* style,
                           std::optional<float> container_content_height) {
    if (!style || !style->height.has_value()) {
        return;
    }
    if (style->height->has_percent && !container_content_height) {
        return;
    }
    float reference = container_content_height.value_or(0.0f);
    float resolved = Metrics::resolve_axis_length(*style->height, reference);
    Metrics::Insets insets = Metrics::compute_insets(style);
    Rect rect = box.get_rect();
    rect.height = std::max(rect.height, Metrics::resolve_border_box_height(style, resolved, insets));
    box.set_rect(rect);
}

}  // namespace

void FlexBox::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    Metrics::BoxMetrics metrics =
        Metrics::compute_box_metrics(style, bounds, m_rect, Metrics::BoxWidthPolicy::WidthAndMax);
    const Metrics::Insets insets = metrics.insets;

    const bool row = !style || style->flex_direction == ComputedStyle::FlexDirection::Row ||
                     style->flex_direction == ComputedStyle::FlexDirection::RowReverse;
    const bool reversed = style && (style->flex_direction == ComputedStyle::FlexDirection::RowReverse ||
                                    style->flex_direction == ComputedStyle::FlexDirection::ColumnReverse);

    const bool wrap = style && style->flex_wrap != ComputedStyle::FlexWrap::NoWrap;
    const bool wrap_reverse = style && style->flex_wrap == ComputedStyle::FlexWrap::WrapReverse;

    const std::optional<float> definite_content_height = resolve_definite_content_height(style, bounds, insets);

    // Collect flex items (absolutely positioned boxes are out of flow).
    std::vector<FlexItem> items;
    items.reserve(m_children.size());
    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        if (Positioning::is_absolute(child_style)) {
            continue;
        }
        FlexItem item;
        item.box = child.get();
        item.margins = FlowLayout::compute_child_margins(child_style, false);
        if (child_style) {
            item.grow = child_style->flex_grow;
            item.shrink = child_style->flex_shrink;
            item.order = child_style->order;
            item.has_explicit_cross = row ? child_style->height.has_value() : child_style->width.has_value();
        }
        items.push_back(item);
    }
    std::stable_sort(items.begin(), items.end(),
                     [](const FlexItem& a, const FlexItem& b) { return a.order < b.order; });

    const float main_available =
        row ? metrics.content_width : (definite_content_height ? *definite_content_height : -1.0f);

    // Pass 1: hypothetical (base) main sizes from basis -> explicit size -> content.
    for (auto& item : items) {
        const auto* child_style = item.box->get_computed_style();
        const Metrics::Insets child_insets = Metrics::compute_insets(child_style);

        std::optional<float> basis;
        if (child_style && child_style->flex_basis.has_value()) {
            const auto& fb = *child_style->flex_basis;
            // A percentage basis needs a definite main size; without one it falls
            // through to explicit size / content, like an auto basis.
            if (!fb.has_percent || main_available >= 0.0f) {
                float value = fb.resolve(main_available);
                basis = row ? Metrics::resolve_border_box_width(child_style, value, child_insets)
                            : Metrics::resolve_border_box_height(child_style, value, child_insets);
            }
        }
        if (!basis && child_style) {
            if (row && child_style->width.has_value()) {
                float resolved = Metrics::resolve_axis_length(*child_style->width, metrics.content_width);
                basis = Metrics::resolve_border_box_width(child_style, resolved, child_insets);
            } else if (!row && child_style->height.has_value() &&
                       !(child_style->height->has_percent && !definite_content_height)) {
                float reference = definite_content_height.value_or(0.0f);
                float resolved = Metrics::resolve_axis_length(*child_style->height, reference);
                basis = Metrics::resolve_border_box_height(child_style, resolved, child_insets);
            }
        }
        if (!basis) {
            if (row) {
                if (is_inline_level(*item.box)) {
                    item.box->layout(context, {0.0f, 0.0f, metrics.content_width, 0.0f});
                    basis = item.box->get_rect().width;
                } else {
                    item.box->layout(context, {0.0f, 0.0f, kIntrinsicMeasureWidth, 0.0f});
                    // A display:block item stretches to fill this oversized probe
                    // box; derive its width from content instead, or it balloons
                    // to ~kIntrinsicMeasureWidth (T-LAYOUT-SHRINK-TO-FIT-1, same
                    // bug as T-LAYOUT-TABLE-INTRINSIC-BLOCK-1).
                    basis = Metrics::max_content_width(*item.box);
                }
            } else {
                float available = std::max(0.0f, metrics.content_width - item.margins.left - item.margins.right);
                item.box->layout(context, {0.0f, 0.0f, available, 0.0f});
                basis = item.box->get_rect().height;
            }
        }

        // Clamp against main-axis min/max constraints where resolvable.
        float clamped = std::max(0.0f, *basis);
        if (child_style) {
            auto resolve_constraint =
                [&](const std::optional<ComputedStyle::LengthValue>& value) -> std::optional<float> {
                if (!value.has_value()) {
                    return std::nullopt;
                }
                float reference = row ? metrics.content_width : definite_content_height.value_or(-1.0f);
                if (value->has_percent && reference < 0.0f) {
                    return std::nullopt;
                }
                float resolved = value->resolve(reference);
                return row ? Metrics::resolve_border_box_width(child_style, resolved, child_insets)
                           : Metrics::resolve_border_box_height(child_style, resolved, child_insets);
            };
            auto min_main =
                row ? resolve_constraint(child_style->min_width) : resolve_constraint(child_style->min_height);
            auto max_main =
                row ? resolve_constraint(child_style->max_width) : resolve_constraint(child_style->max_height);
            if (max_main) {
                clamped = std::min(clamped, *max_main);
            }
            if (min_main) {
                clamped = std::max(clamped, *min_main);
            }
        }
        item.base_main = clamped;
    }

    // Break items into flex lines. Wrapping needs a definite main size to wrap
    // against; an indefinite main axis is treated as a single unbounded line.
    struct FlexLine {
        size_t begin = 0;
        size_t end = 0;
        float cross = 0.0f;
        float baseline = 0.0f;  // shared baseline offset for align-items: baseline
    };
    std::vector<FlexLine> lines;
    if (wrap && main_available >= 0.0f && !items.empty()) {
        size_t start = 0;
        float run = 0.0f;
        for (size_t i = 0; i < items.size(); ++i) {
            const float outer = items[i].base_main + main_margins(items[i], row);
            if (i > start && run + outer > main_available + 0.5f) {
                lines.push_back({start, i, 0.0f});
                start = i;
                run = 0.0f;
            }
            run += outer;
        }
        lines.push_back({start, items.size(), 0.0f});
    } else {
        lines.push_back({0, items.size(), 0.0f});
    }

    const auto align = style ? style->align_items : ComputedStyle::AlignItems::Stretch;
    const auto justify = style ? style->justify_content : ComputedStyle::JustifyContent::FlexStart;
    const float cross_axis_extent = row ? definite_content_height.value_or(0.0f) : metrics.content_width;

    // Phase A: per line, distribute free space and lay each item out at its
    // target main size, then measure the line's cross extent.
    for (auto& line : lines) {
        float sum_outer = 0.0f;
        float sum_grow = 0.0f;
        float sum_scaled_shrink = 0.0f;
        for (size_t i = line.begin; i < line.end; ++i) {
            FlexItem& item = items[i];
            item.target_main = item.base_main;
            sum_outer += item.base_main + main_margins(item, row);
            sum_grow += item.grow;
            sum_scaled_shrink += item.shrink * item.base_main;
        }
        const float container_main = main_available >= 0.0f ? main_available : sum_outer;
        const float free_space = container_main - sum_outer;
        if (free_space > 0.0f && sum_grow > 0.0f) {
            for (size_t i = line.begin; i < line.end; ++i) {
                items[i].target_main = items[i].base_main + free_space * (items[i].grow / sum_grow);
            }
        } else if (free_space < 0.0f && sum_scaled_shrink > 0.0f) {
            for (size_t i = line.begin; i < line.end; ++i) {
                const float ratio = (items[i].shrink * items[i].base_main) / sum_scaled_shrink;
                items[i].target_main = std::max(0.0f, items[i].base_main + free_space * ratio);
            }
        }

        for (size_t i = line.begin; i < line.end; ++i) {
            FlexItem& item = items[i];
            const auto* child_style = item.box->get_computed_style();
            if (row) {
                item.box->layout(context, {0.0f, 0.0f, item.target_main, 0.0f});
                force_rect_main(*item.box, row, item.target_main);
                apply_explicit_height(*item.box, child_style, definite_content_height);
                item.cross = item.box->get_rect().height;
                // First-line baseline, measured from the item's border-box top
                // (approximated from the item's own font metrics).
                const Metrics::Insets child_insets = Metrics::compute_insets(child_style);
                item.baseline_ascent = std::min(
                    item.cross, child_insets.top + InlineBaselineUtils::estimate_text_ascent(context, child_style));
            } else {
                float cross_avail = std::max(0.0f, metrics.content_width - item.margins.left - item.margins.right);
                float layout_width = cross_avail;
                bool has_width = child_style && child_style->width.has_value();
                if (align != ComputedStyle::AlignItems::Stretch && !has_width && !is_inline_level(*item.box)) {
                    item.box->layout(context, {0.0f, 0.0f, kIntrinsicMeasureWidth, 0.0f});
                    layout_width = std::min(cross_avail, Metrics::max_content_width(*item.box));
                }
                item.box->layout(context, {0.0f, 0.0f, layout_width, 0.0f});
                apply_explicit_height(*item.box, child_style, std::nullopt);
                force_rect_main(*item.box, row, item.target_main);
                item.cross = item.box->get_rect().width;
            }
        }

        float measured = 0.0f;
        for (size_t i = line.begin; i < line.end; ++i) {
            measured = std::max(measured, items[i].cross + cross_margins(items[i], row));
        }
        line.cross = measured;

        // For baseline alignment, items shift so their first-line baselines
        // coincide; the line height must cover the deepest resulting extent.
        if (row && align == ComputedStyle::AlignItems::Baseline) {
            float shared = 0.0f;
            for (size_t i = line.begin; i < line.end; ++i) {
                shared = std::max(shared, items[i].margins.top + items[i].baseline_ascent);
            }
            line.baseline = shared;
            float baseline_cross = 0.0f;
            for (size_t i = line.begin; i < line.end; ++i) {
                float top = shared - items[i].baseline_ascent;  // border-box top offset
                baseline_cross = std::max(baseline_cross, top + items[i].cross + items[i].margins.bottom);
            }
            line.cross = std::max(line.cross, baseline_cross);
        }
    }

    // A single line stretches to fill a definite cross extent (so align works
    // against the whole container); multiple lines keep their measured heights.
    float total_cross = 0.0f;
    for (const auto& line : lines) {
        total_cross += line.cross;
    }
    if (lines.size() == 1 && cross_axis_extent > total_cross) {
        lines.front().cross = cross_axis_extent;
        total_cross = cross_axis_extent;
    }

    // Phase B: assign each line a cross band (wrap-reverse stacks bottom-up),
    // then position items along the main axis and align them on the cross axis.
    const float main_start = row ? insets.left : insets.top;
    const float cross_start = row ? insets.top : insets.left;
    float cross_cursor = cross_start;
    for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const FlexLine& line = lines[wrap_reverse ? lines.size() - 1 - line_index : line_index];
        const size_t count = line.end - line.begin;

        float leftover = (main_available >= 0.0f ? main_available : 0.0f);
        if (main_available >= 0.0f) {
            for (size_t i = line.begin; i < line.end; ++i) {
                leftover -= items[i].target_main + main_margins(items[i], row);
            }
        }

        float lead = 0.0f;
        float between = 0.0f;
        if (count > 0) {
            switch (justify) {
                case ComputedStyle::JustifyContent::FlexEnd:
                    lead = leftover;
                    break;
                case ComputedStyle::JustifyContent::Center:
                    lead = leftover / 2.0f;
                    break;
                case ComputedStyle::JustifyContent::SpaceBetween:
                    if (leftover > 0.0f && count > 1) {
                        between = leftover / static_cast<float>(count - 1);
                    }
                    break;
                case ComputedStyle::JustifyContent::SpaceAround:
                    if (leftover > 0.0f) {
                        between = leftover / static_cast<float>(count);
                        lead = between / 2.0f;
                    }
                    break;
                case ComputedStyle::JustifyContent::SpaceEvenly:
                    if (leftover > 0.0f) {
                        between = leftover / static_cast<float>(count + 1);
                        lead = between;
                    }
                    break;
                case ComputedStyle::JustifyContent::FlexStart:
                default:
                    break;
            }
        }

        float cursor = main_start + lead;
        for (size_t offset = 0; offset < count; ++offset) {
            FlexItem& item = items[line.begin + (reversed ? count - 1 - offset : offset)];
            const float main_margin_start = row ? item.margins.left : item.margins.top;
            const float main_margin_end = row ? item.margins.right : item.margins.bottom;
            const float cross_margin_start = row ? item.margins.top : item.margins.left;
            const float cross_margin_end = row ? item.margins.bottom : item.margins.right;

            float cross_offset = cross_cursor + cross_margin_start;
            const float item_cross_extent = item.cross + cross_margin_start + cross_margin_end;
            switch (align) {
                case ComputedStyle::AlignItems::Center:
                    cross_offset += std::max(0.0f, (line.cross - item_cross_extent) / 2.0f);
                    break;
                case ComputedStyle::AlignItems::FlexEnd:
                    cross_offset += std::max(0.0f, line.cross - item_cross_extent);
                    break;
                case ComputedStyle::AlignItems::Stretch:
                    if (row && !item.has_explicit_cross) {
                        Rect rect = item.box->get_rect();
                        float stretched = line.cross - cross_margin_start - cross_margin_end;
                        if (stretched > rect.height) {
                            rect.height = stretched;
                            item.box->set_rect(rect);
                            item.cross = rect.height;
                        }
                    }
                    break;
                case ComputedStyle::AlignItems::Baseline:
                    // Align first-line baselines (row only); shift each item so
                    // its baseline meets the line's shared baseline.
                    if (row) {
                        cross_offset = cross_cursor + std::max(0.0f, line.baseline - item.baseline_ascent);
                    }
                    break;
                case ComputedStyle::AlignItems::FlexStart:
                default:
                    break;
            }

            Rect rect = item.box->get_rect();
            if (row) {
                rect.x = cursor + main_margin_start;
                rect.y = cross_offset;
            } else {
                rect.x = cross_offset;
                rect.y = cursor + main_margin_start;
            }
            item.box->set_rect(rect);
            cursor += main_margin_start + item.target_main + main_margin_end + between;
        }
        cross_cursor += line.cross;
    }

    // Container cross size (height for row, unchanged content model for column).
    if (row) {
        float content_height = definite_content_height ? std::max(*definite_content_height, total_cross) : total_cross;
        m_rect.height = insets.top + content_height + insets.bottom;
    } else {
        float total_main = 0.0f;
        for (auto& item : items) {
            total_main += item.target_main + main_margins(item, row);
        }
        float content_height = definite_content_height ? *definite_content_height : total_main;
        m_rect.height = insets.top + content_height + insets.bottom;
    }

    // Honor min/max height like other boxes.
    if (style) {
        auto resolve_height = [&](const ComputedStyle::LengthValue& value) -> std::optional<float> {
            if (value.has_percent && bounds.height <= 0.0f) {
                return std::nullopt;
            }
            float resolved = value.resolve(bounds.height);
            return Metrics::resolve_border_box_height(style, resolved, insets);
        };
        if (style->min_height.has_value()) {
            if (auto target = resolve_height(*style->min_height)) {
                m_rect.height = std::max(m_rect.height, *target);
            }
        }
        if (style->max_height.has_value()) {
            if (auto target = resolve_height(*style->max_height)) {
                m_rect.height = std::min(m_rect.height, *target);
            }
        }
    }
}

}  // namespace Hummingbird::Layout
