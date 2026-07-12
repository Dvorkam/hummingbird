#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "core/dom/Node.h"
#include "layout/flow/inline/IInlineParticipant.h"
#include "layout/flow/inline/InlineRef.h"
#include "layout/geometry/Geometry.h"
#include "style/types/ComputedStyle.h"

// Forward declare IGraphicsContext to break dependency cycle
namespace Hummingbird {
class IGraphicsContext;
struct ImageBitmap;
namespace Css {
struct ComputedStyle;
}  // namespace Css
namespace Layout {
class IInlineParticipant;
}  // namespace Layout
}  // namespace Hummingbird

namespace Hummingbird::Layout {

struct InlineRun;
struct InlineFragment;

class RenderObject {
public:
    virtual ~RenderObject() = default;

    const DOM::Node* get_dom_node() const { return m_dom_node; }
    const Rect& get_rect() const { return m_rect; }
    void set_rect(const Rect& rect) { m_rect = rect; }
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

    InlineRef Inline() {
        const auto* style = get_computed_style();
        if (style && style->position == Css::ComputedStyle::Position::Absolute) {
            return InlineRef(nullptr);
        }
        return InlineRef(as_inline_participant());
    }

    bool set_background_image(const ImageBitmap* image) {
        if (m_background_image == image) {
            return false;
        }
        m_background_image = image;
        return true;
    }
    const ImageBitmap* background_image() const { return m_background_image; }
    void set_has_absolute_descendant(bool value) { m_has_absolute_descendant = value; }
    bool has_absolute_descendant() const { return m_has_absolute_descendant; }

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
    const ImageBitmap* m_background_image = nullptr;
    bool m_has_absolute_descendant = false;
};
}  // namespace Hummingbird::Layout
