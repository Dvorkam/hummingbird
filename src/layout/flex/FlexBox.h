#pragma once

#include <memory>

#include "layout/block/BlockBox.h"

namespace Hummingbird::Layout {

// Flex container (display: flex). MVP scope: single-line row/column layout with
// justify-content, align-items, and flex grow/shrink/basis resolution.
// flex-wrap and baseline alignment are parsed but not laid out yet.
class FlexBox : public BlockBox {
public:
    static std::unique_ptr<FlexBox> create(const DOM::Node* dom_node) {
        return std::unique_ptr<FlexBox>(new FlexBox(dom_node));
    }

    void layout(IGraphicsContext& context, const Rect& bounds) override;

private:
    explicit FlexBox(const DOM::Node* dom_node) : BlockBox(dom_node) {}
};

}  // namespace Hummingbird::Layout
