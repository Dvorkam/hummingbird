#pragma once

#include <stddef.h>

#include <memory>
#include <vector>

#include "core/ResourceRef.h"
#include "core/dom/Element.h"
#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"
#include "layout/flow/inline/IInlineParticipant.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
struct ImageBitmap;
}

namespace Hummingbird::Layout {

class RenderImage : public RenderObject, public IInlineParticipant {
public:
    static std::unique_ptr<RenderImage> create(const DOM::Element* dom_node) {
        return std::unique_ptr<RenderImage>(new RenderImage(dom_node));
    }

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) const override;

    // The image is named, not held (T-RESOURCE-REF-1). A render object outlives
    // the store's decisions about a resource — decode, replacement, failure,
    // navigation — so holding the pixels was a use-after-free waiting for an
    // animation to widen the window. Sizing and painting both resolve through
    // the graphics context, and the resulting pointer dies with the call.
    bool set_image(ResourceRef image);
    ResourceRef image() const { return m_image_ref; }

    IInlineParticipant* as_inline_participant() override;
    const IInlineParticipant* as_inline_participant() const override;

protected:
    void reset_inline_layout() override;
    void measure_inline(IGraphicsContext& context) override;
    void collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) override;
    void apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) override;
    void finalize_inline_layout() override;
    void offset_inline_layout(float dx, float dy) override {
        m_rect.x += dx;
        m_rect.y += dy;
    }

private:
    explicit RenderImage(const DOM::Element* dom_node) : RenderObject(dom_node) {}

    bool should_inline() const;
    // Resolves the handle through the context, for the duration of one call.
    const ImageBitmap* resolve_image(IGraphicsContext& context) const;
    float m_inline_measured_width = 0.0f;
    float m_inline_measured_height = 0.0f;
    ResourceRef m_image_ref{};
    mutable TextStyle alt_text_style_;
};

}  // namespace Hummingbird::Layout
