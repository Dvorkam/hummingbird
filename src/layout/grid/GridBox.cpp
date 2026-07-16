#include "layout/grid/GridBox.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "layout/flow/FlowLayoutUtils.h"
#include "layout/geometry/PositioningUtils.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Layout {

namespace {

using Css::ComputedStyle;
using GridTrack = ComputedStyle::GridTrack;

struct GridItem {
    RenderObject* box = nullptr;
    FlowLayout::ChildMargins margins;
    int col = 0;  // resolved 0-based column start
    int col_span = 1;
    int row = 0;  // resolved 0-based row start
    int row_span = 1;
    bool has_width = false;
    bool has_height = false;
    float natural_height = 0.0f;
};

// The track that sizes column/row index `i`: the explicit template track if one
// exists, else the implicit fallback (grid-auto-rows for rows; a flexible track
// for columns, which have no auto-columns support in this MVP).
GridTrack track_at(const std::vector<GridTrack>& tracks, int index, GridTrack fallback) {
    if (index >= 0 && index < static_cast<int>(tracks.size())) {
        return tracks[index];
    }
    return fallback;
}

float horizontal_margins(const GridItem& item) {
    return item.margins.left + item.margins.right;
}
float vertical_margins(const GridItem& item) {
    return item.margins.top + item.margins.bottom;
}

}  // namespace

void GridBox::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    Metrics::BoxMetrics metrics =
        Metrics::compute_box_metrics(style, bounds, m_rect, Metrics::BoxWidthPolicy::WidthAndMax);
    const Metrics::Insets insets = metrics.insets;
    const float content_width = metrics.content_width;

    const std::vector<GridTrack> empty_tracks;
    const std::vector<GridTrack>& col_tracks = style ? style->grid_template_columns : empty_tracks;
    const std::vector<GridTrack>& row_tracks = style ? style->grid_template_rows : empty_tracks;
    const GridTrack auto_row = style ? style->grid_auto_rows : GridTrack::automatic();
    const float column_gap = style ? style->column_gap : 0.0f;
    const float row_gap = style ? style->row_gap : 0.0f;

    // A grid always has at least one column; without an explicit template it is a
    // single flexible column (items stack in one column).
    const int num_cols = std::max<int>(1, static_cast<int>(col_tracks.size()));

    // Collect in-flow items and read their placement.
    std::vector<GridItem> items;
    items.reserve(m_children.size());
    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        if (Positioning::is_absolute(child_style)) {
            continue;
        }
        GridItem item;
        item.box = child.get();
        item.margins = FlowLayout::compute_child_margins(child_style, false);
        if (child_style) {
            item.has_width = child_style->width.has_value();
            item.has_height = child_style->height.has_value();
            item.col_span = std::clamp(child_style->grid_column.span, 1, num_cols);
            item.row_span = std::max(1, child_style->grid_row.span);
            // Explicit 1-based lines -> 0-based, or -1 for auto-placement.
            item.col = child_style->grid_column.line > 0 ? child_style->grid_column.line - 1 : -1;
            item.row = child_style->grid_row.line > 0 ? child_style->grid_row.line - 1 : -1;
        }
        items.push_back(item);
    }

    // --- Placement: row-major auto-placement honoring explicit lines/spans. ---
    std::vector<std::vector<char>> occupied;  // occupied[row][col]
    auto ensure_rows = [&](int row) {
        while (static_cast<int>(occupied.size()) <= row) {
            occupied.emplace_back(num_cols, 0);
        }
    };
    auto fits = [&](int row, int col, int rspan, int cspan) {
        if (col < 0 || col + cspan > num_cols || row < 0) {
            return false;
        }
        ensure_rows(row + rspan - 1);
        for (int r = row; r < row + rspan; ++r) {
            for (int c = col; c < col + cspan; ++c) {
                if (occupied[r][c]) return false;
            }
        }
        return true;
    };
    auto mark = [&](int row, int col, int rspan, int cspan) {
        ensure_rows(row + rspan - 1);
        for (int r = row; r < row + rspan; ++r) {
            for (int c = col; c < col + cspan; ++c) {
                occupied[r][c] = 1;
            }
        }
    };

    int cursor_row = 0;
    int cursor_col = 0;
    for (auto& item : items) {
        const int cspan = item.col_span;
        const int rspan = item.row_span;
        int col = item.col;  // -1 = auto
        int row = item.row;  // -1 = auto
        if (col >= 0 && col + cspan > num_cols) {
            col = std::max(0, num_cols - cspan);
        }

        if (col >= 0 && row >= 0) {
            // Fully explicit.
        } else if (col >= 0) {
            row = 0;
            while (!fits(row, col, rspan, cspan)) ++row;
        } else if (row >= 0) {
            col = 0;
            while (col + cspan <= num_cols && !fits(row, col, rspan, cspan)) ++col;
            if (col + cspan > num_cols) col = 0;  // no fit: overlap at column 0 (edge case)
        } else {
            row = cursor_row;
            col = cursor_col;
            while (true) {
                if (col + cspan > num_cols) {
                    col = 0;
                    ++row;
                    continue;
                }
                if (fits(row, col, rspan, cspan)) break;
                ++col;
            }
            cursor_row = row;
            cursor_col = col + cspan;
            if (cursor_col >= num_cols) {
                cursor_col = 0;
                ++cursor_row;
            }
        }
        item.col = col;
        item.row = row;
        mark(row, col, rspan, cspan);
    }

    const int num_rows = std::max<int>(1, static_cast<int>(occupied.size()));

    // --- Column track sizing. Fixed/percent take their size; fr (and auto,
    // treated as 1fr) share the remaining free space. ---
    std::vector<float> col_width(num_cols, 0.0f);
    float sum_fixed_cols = 0.0f;
    float sum_col_fr = 0.0f;
    std::vector<float> col_fr(num_cols, 0.0f);
    for (int c = 0; c < num_cols; ++c) {
        GridTrack track = track_at(col_tracks, c, GridTrack::fr(1.0f));
        switch (track.kind) {
            case GridTrack::Kind::Fixed:
                col_width[c] = std::max(0.0f, track.value);
                sum_fixed_cols += col_width[c];
                break;
            case GridTrack::Kind::Percent:
                col_width[c] = std::max(0.0f, track.value * 0.01f * content_width);
                sum_fixed_cols += col_width[c];
                break;
            case GridTrack::Kind::Fr:
                col_fr[c] = std::max(0.0f, track.value);
                sum_col_fr += col_fr[c];
                break;
            case GridTrack::Kind::Auto:
                col_fr[c] = 1.0f;  // MVP: auto column behaves as 1fr
                sum_col_fr += 1.0f;
                break;
        }
    }
    const float col_gap_total = column_gap * static_cast<float>(num_cols - 1);
    const float col_free = std::max(0.0f, content_width - sum_fixed_cols - col_gap_total);
    for (int c = 0; c < num_cols; ++c) {
        if (col_fr[c] > 0.0f && sum_col_fr > 0.0f) {
            col_width[c] = col_free * (col_fr[c] / sum_col_fr);
        }
    }

    std::vector<float> col_offset(num_cols, 0.0f);
    {
        float x = insets.left;
        for (int c = 0; c < num_cols; ++c) {
            col_offset[c] = x;
            x += col_width[c] + column_gap;
        }
    }
    auto span_width = [&](int col, int cspan) {
        float w = 0.0f;
        for (int c = col; c < col + cspan && c < num_cols; ++c) {
            w += col_width[c];
        }
        return w + column_gap * static_cast<float>(cspan - 1);
    };

    // --- Lay each item out at its cell width to learn its natural height. ---
    for (auto& item : items) {
        const float cell_w = span_width(item.col, item.col_span);
        const float inner_w = std::max(0.0f, cell_w - horizontal_margins(item));
        item.box->layout(context, {0.0f, 0.0f, inner_w, 0.0f});
        item.natural_height = item.box->get_rect().height;
    }

    // --- Row track sizing. Fixed rows take their size; auto/fr rows are sized to
    // their content (single-row items first, then spanning items top up). ---
    std::vector<float> row_height(num_rows, 0.0f);
    std::vector<char> row_fixed(num_rows, 0);
    for (int r = 0; r < num_rows; ++r) {
        GridTrack track = track_at(row_tracks, r, auto_row);
        if (track.kind == GridTrack::Kind::Fixed) {
            row_height[r] = std::max(0.0f, track.value);
            row_fixed[r] = 1;
        } else if (track.kind == GridTrack::Kind::Percent) {
            // Percentage rows need a definite container height, which the grid
            // does not have here; treat as content-sized.
        }
    }
    for (const auto& item : items) {
        if (item.row_span == 1 && item.row < num_rows && !row_fixed[item.row]) {
            row_height[item.row] = std::max(row_height[item.row], item.natural_height + vertical_margins(item));
        }
    }
    for (const auto& item : items) {
        if (item.row_span <= 1) continue;
        float spanned = row_gap * static_cast<float>(item.row_span - 1);
        int last_flexible = -1;
        for (int r = item.row; r < item.row + item.row_span && r < num_rows; ++r) {
            spanned += row_height[r];
            if (!row_fixed[r]) last_flexible = r;
        }
        const float needed = item.natural_height + vertical_margins(item);
        if (needed > spanned && last_flexible >= 0) {
            row_height[last_flexible] += needed - spanned;
        }
    }

    std::vector<float> row_offset(num_rows, 0.0f);
    {
        float y = insets.top;
        for (int r = 0; r < num_rows; ++r) {
            row_offset[r] = y;
            y += row_height[r] + row_gap;
        }
    }
    auto span_height = [&](int row, int rspan) {
        float h = 0.0f;
        for (int r = row; r < row + rspan && r < num_rows; ++r) {
            h += row_height[r];
        }
        return h + row_gap * static_cast<float>(rspan - 1);
    };

    // --- Position + stretch each item into its cell. ---
    for (auto& item : items) {
        const float cell_x = col_offset[std::min(item.col, num_cols - 1)];
        const float cell_y = row_offset[std::min(item.row, num_rows - 1)];
        const float cell_w = span_width(item.col, item.col_span);
        const float cell_h = span_height(item.row, item.row_span);
        const float inner_w = std::max(0.0f, cell_w - horizontal_margins(item));
        const float inner_h = std::max(0.0f, cell_h - vertical_margins(item));

        // Re-lay the item at the definite cell width (its earlier measure used the
        // same width, but this keeps the final rect authoritative).
        item.box->layout(context, {0.0f, 0.0f, inner_w, 0.0f});
        Rect rect = item.box->get_rect();
        rect.x = cell_x + item.margins.left;
        rect.y = cell_y + item.margins.top;
        if (!item.has_width) {
            rect.width = inner_w;  // default justify-self: stretch
        }
        if (!item.has_height && inner_h > rect.height) {
            rect.height = inner_h;  // default align-self: stretch
        }
        item.box->set_rect(rect);
    }

    // --- Container height. ---
    float tracks_height = row_gap * static_cast<float>(num_rows - 1);
    for (int r = 0; r < num_rows; ++r) {
        tracks_height += row_height[r];
    }
    m_rect.height = insets.top + tracks_height + insets.bottom;

    if (style) {
        auto resolve_height = [&](const ComputedStyle::LengthValue& value) -> std::optional<float> {
            if (value.has_percent && bounds.height <= 0.0f) {
                return std::nullopt;
            }
            float resolved = value.resolve(bounds.height);
            return Metrics::resolve_border_box_height(style, resolved, insets);
        };
        if (style->height.has_value()) {
            if (auto target = resolve_height(*style->height)) {
                m_rect.height = std::max(m_rect.height, *target);
            }
        }
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
