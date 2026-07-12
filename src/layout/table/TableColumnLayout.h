#pragma once

#include <stddef.h>

#include <vector>

namespace Hummingbird {
class IGraphicsContext;
}

namespace Hummingbird::Layout {

class RenderTable;
class RenderTableCell;

struct TableColumnLayoutResult {
    std::vector<float> column_widths;
    float content_width = 0.0f;
};

size_t table_cell_colspan(const RenderTableCell& cell);

TableColumnLayoutResult compute_table_column_layout(RenderTable& table, IGraphicsContext& context,
                                                    float available_width);

}  // namespace Hummingbird::Layout
