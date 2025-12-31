#pragma once

#include <cstddef>
#include <vector>

class IGraphicsContext;

namespace Hummingbird::Layout {

struct InlineRun;
struct InlineFragment;

class IInlineParticipant {
public:
    virtual ~IInlineParticipant() = default;

    virtual void reset_inline_layout() = 0;
    virtual void measure_inline(IGraphicsContext& context) = 0;
    virtual void collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) = 0;
    virtual void apply_inline_fragment(size_t local_index, const InlineFragment& fragment, const InlineRun& run) = 0;
    virtual void finalize_inline_layout() = 0;
    virtual void offset_inline_layout(float dx, float dy) = 0;
};

}  // namespace Hummingbird::Layout
