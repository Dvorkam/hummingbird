#pragma once

#include <memory>

#include "core/dom/Node.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class TreeBuilder {
public:
    template <DOM::NodeLike NodeT>
    std::unique_ptr<RenderObject> build(const NodeT* dom_root) {
        return build_impl(static_cast<const DOM::Node*>(dom_root));
    }

private:
    std::unique_ptr<RenderObject> build_impl(const DOM::Node* dom_root);
};

}  // namespace Hummingbird::Layout
