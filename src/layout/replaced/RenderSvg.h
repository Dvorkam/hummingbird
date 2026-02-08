#pragma once

#include <memory>

#include "core/dom/Element.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "layout/RenderObject.h"
#include "layout/flow/inline/IInlineParticipant.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::Layout {

class RenderSvg : public RenderObject, public IInlineParticipant {
public:
    static std::unique_ptr<RenderSvg> create(const DOM::Element* dom_node) {
        return std::unique_ptr<RenderSvg>(new RenderSvg(dom_node));
    }

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) const override;

    bool set_image(std::unique_ptr<ImageBitmap> image);
    const ImageBitmap* image() const { return m_image.get(); }

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
    explicit RenderSvg(const DOM::Element* dom_node) : RenderObject(dom_node) {}

    bool should_inline() const;

    float m_inline_measured_width = 0.0f;
    float m_inline_measured_height = 0.0f;
    std::unique_ptr<ImageBitmap> m_image;
};

}  // namespace Hummingbird::Layout
