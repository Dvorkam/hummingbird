#pragma once

#include <string>

#include "core/dom/Text.h"
#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"
#include "layout/inline/IInlineParticipant.h"

namespace Hummingbird::Layout {

class TextBox : public RenderObject, public IInlineParticipant {
public:
    TextBox(const DOM::Text* dom_node);

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) override;
    const std::string& rendered_text() const { return m_rendered_text; }

    const DOM::Text* get_dom_node() const { return static_cast<const DOM::Text*>(m_dom_node); }

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
    struct TextFragment {
        std::string text;
        Rect rect;
        size_t line_index = 0;
    };

    std::string m_rendered_text;
    std::vector<std::string> m_lines;
    std::vector<TextFragment> m_fragments;
    float m_line_height = 0.0f;
    TextMetrics m_last_metrics{};
};

}  // namespace Hummingbird::Layout
