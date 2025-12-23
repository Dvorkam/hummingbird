#include "layout/InlineBox.h"

#include <algorithm>

#include "core/IGraphicsContext.h"
#include "layout/InlineLineBuilder.h"

namespace Hummingbird::Layout {

void InlineBox::reset_inline_layout() {
    m_inline_atomic = false;
}

void InlineBox::collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) {
    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float border_top = style ? style->border_width.top : 0.0f;
    float border_bottom = style ? style->border_width.bottom : 0.0f;

    bool has_insets = padding_left > 0.0f || padding_right > 0.0f || padding_top > 0.0f || padding_bottom > 0.0f ||
                      border_left > 0.0f || border_right > 0.0f || border_top > 0.0f || border_bottom > 0.0f;

    if (has_insets) {
        m_inline_atomic = true;
        layout(context, {0.0f, 0.0f, 100000.0f, 0.0f});
        InlineRun run;
        run.owner = this;
        run.local_index = 0;
        run.width = m_rect.width;
        run.height = m_rect.height;
        runs.push_back(std::move(run));
        return;
    }

    m_inline_atomic = false;
    for (auto& child : m_children) {
        child->reset_inline_layout();
        child->collect_inline_runs(context, runs);
    }
}

void InlineBox::apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) {
    if (!m_inline_atomic || index != 0) {
        return;
    }
    m_rect.x = fragment.rect.x;
    m_rect.y = fragment.rect.y;
    m_rect.width = run.width;
    m_rect.height = run.height;
}

void InlineBox::finalize_inline_layout() {
    if (m_inline_atomic) {
        return;
    }

    if (m_children.empty()) {
        m_rect = {};
        return;
    }

    bool has_bounds = false;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;

    for (const auto& child : m_children) {
        child->finalize_inline_layout();
        const auto& rect = child->get_rect();
        if (!has_bounds) {
            min_x = rect.x;
            min_y = rect.y;
            max_x = rect.x + rect.width;
            max_y = rect.y + rect.height;
            has_bounds = true;
            continue;
        }
        min_x = std::min(min_x, rect.x);
        min_y = std::min(min_y, rect.y);
        max_x = std::max(max_x, rect.x + rect.width);
        max_y = std::max(max_y, rect.y + rect.height);
    }

    if (!has_bounds) {
        m_rect = {};
        return;
    }

    m_rect.x = min_x;
    m_rect.y = min_y;
    m_rect.width = max_x - min_x;
    m_rect.height = max_y - min_y;

    for (auto& child : m_children) {
        child->offset_inline_layout(-min_x, -min_y);
    }
}

void InlineBox::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;

    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float border_top = style ? style->border_width.top : 0.0f;
    float border_bottom = style ? style->border_width.bottom : 0.0f;

    float inset_left = padding_left + border_left;
    float inset_right = padding_right + border_right;
    float inset_top = padding_top + border_top;
    float inset_bottom = padding_bottom + border_bottom;

    float content_width = bounds.width - inset_left - inset_right;
    if (content_width < 0.0f) content_width = 0.0f;

    float cursor_x = inset_left;
    float cursor_y = inset_top;
    float line_height = 0.0f;

    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        float margin_left = child_style ? child_style->margin.left : 0.0f;
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float margin_top = child_style ? child_style->margin.top : 0.0f;
        float margin_bottom = child_style ? child_style->margin.bottom : 0.0f;

        float child_x = cursor_x + margin_left;
        float child_y = cursor_y + margin_top;
        float available_width = content_width - (child_x - inset_left);
        Rect child_bounds{child_x, child_y, available_width, 0.0f};
        child->layout(context, child_bounds);

        float child_height = child->get_rect().height + margin_top + margin_bottom;
        line_height = std::max(line_height, child_height);
        cursor_x = child_x + child->get_rect().width + margin_right;
    }

    m_rect.width = cursor_x + inset_right;
    m_rect.height = cursor_y + line_height + inset_bottom;
}

void InlineBox::paint(IGraphicsContext& context, const Point& offset) {
    RenderObject::paint(context, offset);
}

}  // namespace Hummingbird::Layout
