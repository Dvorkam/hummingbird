#pragma once

#include "core/dom/Node.h"
#include "layout/RenderObject.h"
#include <memory>

namespace Hummingbird::Layout {

    class TreeBuilder {
    public:
        template <DOM::NodeLike NodeT>
        std::unique_ptr<RenderObject> build(const NodeT* dom_root) {
            return build_impl(static_cast<const DOM::Node*>(dom_root));
        }

    private:
        std::unique_ptr<RenderObject> build_impl(const DOM::Node* dom_root);
        std::unique_ptr<RenderObject> create_render_object(const DOM::Node* node);
    };

}
