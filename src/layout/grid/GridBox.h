#pragma once

#include <memory>

#include "layout/block/BlockBox.h"

namespace Hummingbird::Layout {

// Grid container (display: grid). MVP scope (T-LAYOUT-GRID-1): fixed/fr/%/auto
// column and row tracks (repeat() expanded at parse), row/column gaps, row-major
// auto-placement, and line/span item placement (grid-column / grid-row). Items
// stretch to fill their cell by default. Not covered: minmax(), auto-fill/fit,
// named lines/areas, grid-auto-flow: column, and content-sized `auto` tracks
// (auto is treated as a flexible 1fr track for now).
class GridBox : public BlockBox {
public:
    static std::unique_ptr<GridBox> create(const DOM::Node* dom_node) {
        return std::unique_ptr<GridBox>(new GridBox(dom_node));
    }

    void layout(IGraphicsContext& context, const Rect& bounds) override;

private:
    explicit GridBox(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

}  // namespace Hummingbird::Layout
