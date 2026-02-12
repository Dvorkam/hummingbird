#pragma once

#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
}

namespace Hummingbird::Layout {
class RenderTableCell;

void paint_table_cell_fallback_border(const RenderTableCell& cell, IGraphicsContext& context, const Point& offset);

}  // namespace Hummingbird::Layout
