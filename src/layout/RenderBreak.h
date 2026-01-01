#pragma once

#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class RenderBreak : public RenderObject {
public:
    static std::unique_ptr<RenderBreak> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderBreak>(new RenderBreak(dom_node));
    }
    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) const override;

private:
    explicit RenderBreak(const DOM::Node* dom_node) : RenderObject(dom_node) {}
};

}  // namespace Hummingbird::Layout
