#pragma once

#include <memory>

#include "layout/RenderObject.h"

namespace Hummingbird {
namespace DOM {
class Node;
}  // namespace DOM
}  // namespace Hummingbird

namespace Hummingbird::Layout {

class RenderRule : public RenderObject {
public:
    static std::unique_ptr<RenderRule> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderRule>(new RenderRule(dom_node));
    }
    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) const override;

private:
    explicit RenderRule(const DOM::Node* dom_node) : RenderObject(dom_node) {}
};

}  // namespace Hummingbird::Layout
