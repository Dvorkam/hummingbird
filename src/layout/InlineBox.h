#pragma once

#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class InlineBox : public RenderObject {
public:
    using RenderObject::RenderObject;

    void layout(IGraphicsContext& context, const Rect& bounds) override;

protected:
    bool is_inline() const override { return true; }
    void reset_inline_layout() override;
    void collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) override;
    void apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) override;
    void finalize_inline_layout() override;

private:
    bool m_inline_atomic = false;
};

}  // namespace Hummingbird::Layout
