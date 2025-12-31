#pragma once

#include "layout/RenderObject.h"
#include "layout/inline/IInlineParticipant.h"

namespace Hummingbird::Layout {

class InlineBox : public RenderObject, public IInlineParticipant {
public:
    using RenderObject::RenderObject;

    void layout(IGraphicsContext& context, const Rect& bounds) override;

    IInlineParticipant* as_inline_participant() override { return this; }
    const IInlineParticipant* as_inline_participant() const override { return this; }

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
    bool m_inline_atomic = false;
    float m_inline_measured_width = 0.0f;
    float m_inline_measured_height = 0.0f;
};

}  // namespace Hummingbird::Layout
