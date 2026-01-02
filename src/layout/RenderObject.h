#pragma once

#include <memory>
#include <vector>

#include "core/dom/Node.h"
#include "layout/Geometry.h"
#include "layout/inline/IInlineParticipant.h"
#include "layout/inline/InlineRef.h"
#include "style/ComputedStyle.h"

// Forward declare IGraphicsContext to break dependency cycle
class IGraphicsContext;

namespace Hummingbird::Layout {

struct InlineRun;
struct InlineFragment;

class RenderObject {
public:
    virtual ~RenderObject() = default;

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
    RenderObject* get_parent() { return m_parent; }
    const RenderObject* get_parent() const { return m_parent; }

    InlineRef Inline() { return InlineRef(as_inline_participant()); }

    virtual void layout(IGraphicsContext& context, const Rect& bounds);
    virtual void paint(IGraphicsContext& context, const Point& offset) const final;
    virtual void paint_self(IGraphicsContext& context, const Point& offset) const;

protected:
    explicit RenderObject(const DOM::Node* dom_node) : m_dom_node(dom_node) {}

    virtual IInlineParticipant* as_inline_participant() { return nullptr; }
    virtual const IInlineParticipant* as_inline_participant() const { return nullptr; }
    const DOM::Node* m_dom_node;  // Non-owning pointer
    RenderObject* m_parent = nullptr;
    std::vector<std::unique_ptr<RenderObject>> m_children;
    Rect m_rect;
};
}  // namespace Hummingbird::Layout
