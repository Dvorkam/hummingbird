#pragma once

#include "layout/RenderObject.h"
#include "core/dom/Text.h"

namespace Hummingbird::Layout {

    class TextBox : public RenderObject {
    public:
        TextBox(const DOM::Text* dom_node);

        void layout(IGraphicsContext& context, const Rect& bounds) override;
        void paint(IGraphicsContext& context, const Point& offset) override;

        const DOM::Text* get_dom_node() const {
            return static_cast<const DOM::Text*>(m_dom_node);
        }
    };

}