#pragma once

#include <memory>
#include <vector>

#include "core/dom/Node.h"
#include "style/ComputedStyle.h"

// Forward declare IGraphicsContext to break dependency cycle
class IGraphicsContext;

namespace Hummingbird::Layout {

struct Point {
    float x = 0, y = 0;
};

struct Rect {
    float x = 0, y = 0, width = 0, height = 0;
};

struct InlineRun;
struct InlineFragment;

class RenderObject {
public:
    RenderObject(const DOM::Node* dom_node) : m_dom_node(dom_node) {}
    virtual ~RenderObject() = default;

    virtual bool is_inline() const { return false; }
    virtual void reset_inline_layout() {}
    virtual void collect_inline_runs(IGraphicsContext& /*context*/, std::vector<InlineRun>& /*runs*/) {}
    virtual void apply_inline_fragment(size_t /*index*/, const InlineFragment& /*fragment*/, const InlineRun& /*run*/) {
    }
    virtual void finalize_inline_layout() {}
    virtual void offset_inline_layout(float dx, float dy) {
        m_rect.x += dx;
        m_rect.y += dy;
    }
    const DOM::Node* get_dom_node() const { return m_dom_node; }
    const Rect& get_rect() const { return m_rect; }
    const Css::ComputedStyle* get_computed_style() const {
        auto style = m_dom_node ? m_dom_node->get_computed_style() : nullptr;
        return style ? style.get() : nullptr;
    }

    void append_child(std::unique_ptr<RenderObject> child) {
        child->m_parent = this;
        m_children.push_back(std::move(child));
    }

    const std::vector<std::unique_ptr<RenderObject>>& get_children() const { return m_children; }
    RenderObject* get_parent() const { return m_parent; }

    virtual void layout(IGraphicsContext& context, const Rect& bounds);
    virtual void paint(IGraphicsContext& context, const Point& offset) final;
    virtual void paint_self(IGraphicsContext& context, const Point& offset);

protected:
    const DOM::Node* m_dom_node;  // Non-owning pointer
    RenderObject* m_parent = nullptr;
    std::vector<std::unique_ptr<RenderObject>> m_children;
    Rect m_rect;
};

}  // namespace Hummingbird::Layout
