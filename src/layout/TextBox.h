#pragma once

#include "core/IGraphicsContext.h"
#include "core/dom/Text.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class TextBox : public RenderObject {
public:
    TextBox(const DOM::Text* dom_node);

    bool is_inline() const override { return true; }
    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint(IGraphicsContext& context, const Point& offset) override;
    const std::string& rendered_text() const { return m_rendered_text; }

    const DOM::Text* get_dom_node() const { return static_cast<const DOM::Text*>(m_dom_node); }

private:
    std::string m_rendered_text;
    TextMetrics m_last_metrics{};
};

}  // namespace Hummingbird::Layout
