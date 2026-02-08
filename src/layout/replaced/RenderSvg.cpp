#include "layout/replaced/RenderSvg.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/dom/ElementUtils.h"
#include "core/platform_api/IImageDecoder.h"
#include "html/HtmlAttributeNames.h"
#include "layout/flow/inline/InlineTypes.h"
#include "layout/geometry/metrics/ReplacedElementUtils.h"
#include "layout/paint/PaintUtils.h"
#include "layout/replaced/ReplacedSizingUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kDefaultSvgWidth = 300.0f;
constexpr float kDefaultSvgHeight = 150.0f;

const Color kPlaceholderFill{230, 230, 230, 255};
const Color kPlaceholderStroke{150, 150, 150, 255};

}  // namespace

void RenderSvg::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    auto* element = static_cast<const DOM::Element*>(get_dom_node());
    const auto* style = get_computed_style();
    ReplacedSizing::IntrinsicSize intrinsic;
    if (m_image && m_image->width > 0) {
        intrinsic.width = static_cast<float>(m_image->width);
        intrinsic.has_width = true;
    }
    if (m_image && m_image->height > 0) {
        intrinsic.height = static_cast<float>(m_image->height);
        intrinsic.has_height = true;
    }
    ReplacedElementUtils::LayoutSize size =
        ReplacedSizing::compute_layout_size(*element, style, kDefaultSvgWidth, kDefaultSvgHeight, intrinsic);

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = size.width;
    m_rect.height = size.height;
}

void RenderSvg::paint_self(IGraphicsContext& context, const Point& offset) const {
    RenderObject::paint_self(context, offset);

    const auto* style = get_computed_style();
    Metrics::Insets insets = Metrics::compute_insets(style);
    Rect content{offset.x + m_rect.x + insets.left, offset.y + m_rect.y + insets.top,
                 m_rect.width - insets.left - insets.right, m_rect.height - insets.top - insets.bottom};

    if (content.width <= 0.0f || content.height <= 0.0f) {
        return;
    }

    if (m_image) {
        context.draw_image(*m_image, content);
        return;
    }

    bool fill_placeholder = !(style && style->background.has_value());
    PaintUtils::draw_placeholder_box(context, content, kPlaceholderFill, kPlaceholderStroke, fill_placeholder);
}

IInlineParticipant* RenderSvg::as_inline_participant() {
    return should_inline() ? this : nullptr;
}

const IInlineParticipant* RenderSvg::as_inline_participant() const {
    return should_inline() ? this : nullptr;
}

void RenderSvg::reset_inline_layout() {
    m_inline_measured_width = 0.0f;
    m_inline_measured_height = 0.0f;
}

void RenderSvg::measure_inline(IGraphicsContext& /*context*/) {
    auto* element = static_cast<const DOM::Element*>(get_dom_node());
    const auto* style = get_computed_style();
    ReplacedSizing::IntrinsicSize intrinsic;
    if (m_image && m_image->width > 0) {
        intrinsic.width = static_cast<float>(m_image->width);
        intrinsic.has_width = true;
    }
    if (m_image && m_image->height > 0) {
        intrinsic.height = static_cast<float>(m_image->height);
        intrinsic.has_height = true;
    }
    ReplacedElementUtils::LayoutSize size =
        ReplacedSizing::compute_layout_size(*element, style, kDefaultSvgWidth, kDefaultSvgHeight, intrinsic);
    m_inline_measured_width = size.width;
    m_inline_measured_height = size.height;
}

void RenderSvg::collect_inline_runs(IGraphicsContext& /*context*/, std::vector<InlineRun>& runs) {
    InlineRun run;
    run.owner = this;
    run.local_index = 0;
    run.width = m_inline_measured_width;
    run.height = m_inline_measured_height;
    run.ascent = m_inline_measured_height;
    runs.push_back(std::move(run));
}

void RenderSvg::apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) {
    if (index != 0) {
        return;
    }
    m_rect.x = fragment.rect.x;
    m_rect.y = fragment.rect.y;
    m_rect.width = run.width;
    m_rect.height = run.height;
}

void RenderSvg::finalize_inline_layout() {}

bool RenderSvg::set_image(std::unique_ptr<ImageBitmap> image) {
    const bool had_image = static_cast<bool>(m_image);
    const bool has_image = static_cast<bool>(image);
    if (had_image && has_image) {
        if (m_image->width == image->width && m_image->height == image->height) {
            m_image = std::move(image);
            return false;
        }
    }
    m_image = std::move(image);
    return had_image != has_image || has_image;
}

bool RenderSvg::should_inline() const {
    const auto* style = get_computed_style();
    auto display = style ? style->display : Css::ComputedStyle::Display::Inline;
    return display == Css::ComputedStyle::Display::Inline || display == Css::ComputedStyle::Display::InlineBlock;
}

}  // namespace Hummingbird::Layout
