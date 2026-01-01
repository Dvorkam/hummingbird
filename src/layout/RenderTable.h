#pragma once

#include "layout/BlockBox.h"

namespace Hummingbird::Layout {

class RenderTable : public BlockBox {
public:
    static std::unique_ptr<RenderTable> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderTable>(new RenderTable(dom_node));
    }

private:
    explicit RenderTable(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

class RenderTableSection : public BlockBox {
public:
    static std::unique_ptr<RenderTableSection> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderTableSection>(new RenderTableSection(dom_node));
    }

private:
    explicit RenderTableSection(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

class RenderTableRow : public BlockBox {
public:
    static std::unique_ptr<RenderTableRow> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderTableRow>(new RenderTableRow(dom_node));
    }

private:
    explicit RenderTableRow(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

class RenderTableCell : public BlockBox {
public:
    static std::unique_ptr<RenderTableCell> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderTableCell>(new RenderTableCell(dom_node));
    }

private:
    explicit RenderTableCell(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

}  // namespace Hummingbird::Layout
