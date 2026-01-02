#pragma once

#include "layout/BlockBox.h"

namespace Hummingbird::Layout {

class RenderTable : public BlockBox {
public:
    static std::unique_ptr<RenderTable> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderTable>(new RenderTable(dom_node));
    }

    void layout(IGraphicsContext& context, const Rect& bounds) override;

private:
    explicit RenderTable(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

class RenderTableSection : public BlockBox {
public:
    static std::unique_ptr<RenderTableSection> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderTableSection>(new RenderTableSection(dom_node));
    }

    void layout_rows(IGraphicsContext& context, const Rect& bounds, const std::vector<float>& column_widths);

private:
    explicit RenderTableSection(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

class RenderTableRow : public BlockBox {
public:
    static std::unique_ptr<RenderTableRow> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderTableRow>(new RenderTableRow(dom_node));
    }

    void layout_row(IGraphicsContext& context, const Rect& bounds, const std::vector<float>& column_widths);

private:
    explicit RenderTableRow(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

class RenderTableCell : public BlockBox {
public:
    static std::unique_ptr<RenderTableCell> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderTableCell>(new RenderTableCell(dom_node));
    }

    float measure_intrinsic_width(IGraphicsContext& context);

private:
    explicit RenderTableCell(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

}  // namespace Hummingbird::Layout
