#pragma once

#include "layout/RenderObject.h"
#include "layout/inline/IInlineParticipant.h"

namespace Hummingbird::Layout {

class BlockBox : public RenderObject {
public:
    using RenderObject::RenderObject;  // Inherit constructor

    void layout(IGraphicsContext& context, const Rect& bounds) override;
};

class InlineBlockBox : public BlockBox, public IInlineParticipant {
public:
    using BlockBox::BlockBox;

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    IInlineParticipant* as_inline_participant() override { return this; }
    const IInlineParticipant* as_inline_participant() const override { return this; }

protected:
    void reset_inline_layout() override;
    void collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) override;
    void apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) override;
    void finalize_inline_layout() override;
    void offset_inline_layout(float dx, float dy) override {
        m_rect.x += dx;
        m_rect.y += dy;
    }

private:
    bool m_inline_atomic = false;
};

}  // namespace Hummingbird::Layout
