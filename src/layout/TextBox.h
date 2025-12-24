#pragma once

#include <string>

#include "core/IGraphicsContext.h"
#include "core/dom/Text.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class TextBox : public RenderObject {
public:
    TextBox(const DOM::Text* dom_node);

    bool is_inline() const override { return true; }
    void reset_inline_layout() override;
    void collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) override;
    void apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) override;
    void finalize_inline_layout() override;
    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) override;
    const std::string& rendered_text() const { return m_rendered_text; }

    const DOM::Text* get_dom_node() const { return static_cast<const DOM::Text*>(m_dom_node); }

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
