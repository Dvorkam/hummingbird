#pragma once

#include "core/utils/Assert.h"
#include "layout/inline/IInlineParticipant.h"

namespace Hummingbird::Layout {

class InlineRef {
public:
    explicit InlineRef(IInlineParticipant* p) : p_(p) {}

    explicit operator bool() const { return p_ != nullptr; }

    IInlineParticipant& get() {
        HB_ASSERT((p_ != nullptr));
        return *p_;
    }

    const IInlineParticipant& get() const {
        HB_ASSERT((p_ != nullptr));
        return *p_;
    }

private:
    IInlineParticipant* p_ = nullptr;
};

}  // namespace Hummingbird::Layout
