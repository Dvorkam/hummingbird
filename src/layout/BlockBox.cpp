#include "layout/BlockBox.h"

#include <algorithm>

#include "layout/InlineLineBuilder.h"

namespace Hummingbird::Layout {

void BlockBox::layout(IGraphicsContext& context, const Rect& bounds) {
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

    float target_width = bounds.width;
    if (style && style->width.has_value()) {
        target_width = std::min(bounds.width, *style->width);
    }

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    if (style && style->width.has_value()) {
        m_rect.width = target_width + inset_left + inset_right;
    } else {
        m_rect.width = bounds.width;
    }

    float content_width = m_rect.width - inset_left - inset_right;
    float cursor_x = inset_left;
    float cursor_y = inset_top;
    float line_height = 0.0f;

    auto flush_line = [&]() {
        cursor_y += line_height;
        cursor_x = inset_left;
        line_height = 0.0f;
    };

    size_t i = 0;
    while (i < m_children.size()) {
        auto& child = m_children[i];
        const auto* child_style = child->get_computed_style();
        float margin_left = child_style ? child_style->margin.left : 0.0f;
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float margin_top = child_style ? child_style->margin.top : 0.0f;
        float margin_bottom = child_style ? child_style->margin.bottom : 0.0f;

        bool is_inline = child->is_inline();

        if (!is_inline) {
            // Control objects like <br> need to break the line before stacking blocks.
            if (child_style && margin_top > 0.0f) {
                cursor_y += margin_top;
            }
            flush_line();
            float child_x = inset_left + margin_left;
            float child_y = cursor_y;
            float available_width = content_width - margin_left - margin_right;
            Rect child_bounds = {child_x, child_y, available_width, 0.0f};  // Height determined by child
            child->layout(context, child_bounds);
            cursor_y = child_y + child->get_rect().height + margin_bottom;
            ++i;
            continue;
        }

        InlineLineBuilder builder;
        builder.reset();
        std::vector<InlineRun> runs;
        size_t group_start = i;

        while (i < m_children.size() && m_children[i]->is_inline()) {
            auto& inline_child = m_children[i];
            inline_child->reset_inline_layout();
            inline_child->collect_inline_runs(context, runs);
            ++i;
        }

        if (runs.empty()) {
            continue;
        }

        for (const auto& run : runs) {
            builder.add_run(run);
        }

        float start_x = cursor_x - inset_left;
        auto fragments = builder.layout(content_width, start_x);
        float base_x = inset_left;
        float base_y = cursor_y;

        for (auto& fragment : fragments) {
            fragment.rect.x += base_x;
            fragment.rect.y += base_y;
            auto& run = runs[fragment.run_index];
            if (run.owner) {
                run.owner->apply_inline_fragment(run.local_index, fragment, run);
            }
        }

        for (size_t j = group_start; j < i; ++j) {
            m_children[j]->finalize_inline_layout();
        }

        const auto& heights = builder.line_heights();
        if (!heights.empty()) {
            float total_height = 0.0f;
            for (float h : heights) total_height += h;
            float last_height = heights.back();
            size_t last_line = heights.size() - 1;
            float last_line_width = 0.0f;
            for (const auto& fragment : fragments) {
                if (fragment.line_index == last_line) {
                    last_line_width = std::max(last_line_width, fragment.rect.x + fragment.rect.width - base_x);
                }
            }
            cursor_y = base_y + (total_height - last_height);
            cursor_x = inset_left + last_line_width;
            line_height = std::max(line_height, last_height);
        }
    }

    flush_line();
    m_rect.height = cursor_y + inset_bottom;
}

void InlineBlockBox::reset_inline_layout() {
    m_inline_atomic = false;
}

void InlineBlockBox::collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) {
    m_inline_atomic = true;
    layout(context, {0.0f, 0.0f, 100000.0f, 0.0f});
    InlineRun run;
    run.owner = this;
    run.local_index = 0;
    run.width = m_rect.width;
    run.height = m_rect.height;
    runs.push_back(std::move(run));
}

void InlineBlockBox::apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) {
    if (!m_inline_atomic || index != 0) {
        return;
    }
    m_rect.x = fragment.rect.x;
    m_rect.y = fragment.rect.y;
    m_rect.width = run.width;
    m_rect.height = run.height;
}

void InlineBlockBox::finalize_inline_layout() {
    if (!m_inline_atomic) {
        m_rect = {};
    }
}

void InlineBlockBox::layout(IGraphicsContext& context, const Rect& bounds) {
    BlockBox::layout(context, bounds);

    const auto* style = get_computed_style();
    if (style && style->width.has_value()) {
        return;
    }

    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float inset_left = padding_left + border_left;
    float inset_right = padding_right + border_right;

    float content_right = inset_left;
    for (const auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float right = child->get_rect().x + child->get_rect().width + margin_right;
        content_right = std::max(content_right, right);
    }

    float required_width = content_right + inset_right;
    if (required_width < inset_left + inset_right) {
        required_width = inset_left + inset_right;
    }

    m_rect.width = std::min(m_rect.width, required_width);
}

}  // namespace Hummingbird::Layout
