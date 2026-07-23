#include "layout/replaced/RenderImage.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/dom/ElementUtils.h"
#include "core/platform_api/IImageDecoder.h"
#include "core/utils/AssetPath.h"
#include "html/HtmlAttributeNames.h"
#include "layout/flow/inline/InlineTypes.h"
#include "layout/flow/inline/InlineVerticalAlignUtils.h"
#include "layout/geometry/metrics/ReplacedElementUtils.h"
#include "layout/paint/PaintUtils.h"
#include "layout/replaced/ObjectFitUtils.h"
#include "layout/replaced/ReplacedSizingUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kDefaultImageWidth = 300.0f;
constexpr float kDefaultImageHeight = 150.0f;
constexpr float kAltTextPadding = 4.0f;

const Color kPlaceholderFill{230, 230, 230, 255};
const Color kPlaceholderStroke{150, 150, 150, 255};

const std::string& resolve_default_font_path() {
    static const std::string kDefaultFontPath =
        Hummingbird::Core::Utils::resolve_asset_path_string("assets/fonts/Roboto-Regular.ttf");
    return kDefaultFontPath;
}
}  // namespace

void RenderImage::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
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
    // Resolve percentage dimensions against the containing block (story 8.5.1).
    // A non-positive extent is indefinite, so a percentage against it is treated
    // as auto — same guard as BlockBox::resolve_height_constraint.
    const std::optional<float> cb_w = bounds.width > 0.0f ? std::optional<float>(bounds.width) : std::nullopt;
    const std::optional<float> cb_h = bounds.height > 0.0f ? std::optional<float>(bounds.height) : std::nullopt;
    ReplacedElementUtils::LayoutSize size = ReplacedSizing::compute_layout_size(
        *element, style, kDefaultImageWidth, kDefaultImageHeight, intrinsic, cb_w, cb_h);

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = size.width;
    m_rect.height = size.height;
}

void RenderImage::paint_self(IGraphicsContext& context, const Point& offset) const {
    RenderObject::paint_self(context, offset);

    auto* element = static_cast<const DOM::Element*>(get_dom_node());
    const auto* style = get_computed_style();
    Metrics::Insets insets = Metrics::compute_insets(style);

    Rect content{offset.x + m_rect.x + insets.left, offset.y + m_rect.y + insets.top,
                 m_rect.width - insets.left - insets.right, m_rect.height - insets.top - insets.bottom};

    if (content.width <= 0.0f || content.height <= 0.0f) {
        return;
    }

    if (m_image) {
        // object-fit (story 8.5.2): fit the intrinsic pixels inside the content
        // box; a cover/none image that exceeds the box is clipped to it.
        const auto fit = style ? style->object_fit : Css::ComputedStyle::ObjectFit::Fill;
        const auto placement = ObjectFitUtils::compute_fit(fit, content, static_cast<float>(m_image->width),
                                                           static_cast<float>(m_image->height));
        if (placement.needs_clip) {
            context.push_clip(content);
            context.draw_image(*m_image, placement.dest);
            context.pop_clip();
        } else {
            context.draw_image(*m_image, placement.dest);
        }
        return;
    }

    bool fill_placeholder = !(style && style->background.has_value());
    PaintUtils::draw_placeholder_box(context, content, kPlaceholderFill, kPlaceholderStroke, fill_placeholder);

    std::string alt_text;
    if (const auto value = DOM::find_attribute_value(*element, Hummingbird::Html::AttributeNames::Alt)) {
        alt_text.assign(*value);
    }
    if (alt_text.empty()) {
        return;
    }

    TextStyle& text_style = alt_text_style_;
    if (text_style.font_path.empty()) {
        text_style.font_path = resolve_default_font_path();
    }
    text_style.font_size = style ? style->font_size : 12.0f;
    text_style.bold = false;
    text_style.italic = false;
    text_style.monospace = false;
    text_style.color = style ? style->color : Color{80, 80, 80, 255};

    float text_x = content.x + kAltTextPadding;
    float text_y = content.y + kAltTextPadding;
    context.draw_text(alt_text, text_x, text_y, text_style);
}

IInlineParticipant* RenderImage::as_inline_participant() {
    return should_inline() ? this : nullptr;
}

const IInlineParticipant* RenderImage::as_inline_participant() const {
    return should_inline() ? this : nullptr;
}

void RenderImage::reset_inline_layout() {
    m_inline_measured_width = 0.0f;
    m_inline_measured_height = 0.0f;
}

void RenderImage::measure_inline(IGraphicsContext& /*context*/) {
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
        ReplacedSizing::compute_layout_size(*element, style, kDefaultImageWidth, kDefaultImageHeight, intrinsic);
    m_inline_measured_width = size.width;
    m_inline_measured_height = size.height;
}

void RenderImage::collect_inline_runs(IGraphicsContext& /*context*/, std::vector<InlineRun>& runs) {
    const auto* style = get_computed_style();
    InlineRun run;
    run.owner = this;
    run.local_index = 0;
    run.width = m_inline_measured_width;
    run.height = m_inline_measured_height;
    run.ascent = m_inline_measured_height;
    run.vertical_align = resolve_inline_vertical_align(style);
    runs.push_back(std::move(run));
}

void RenderImage::apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) {
    if (index != 0) {
        return;
    }
    m_rect.x = fragment.rect.x;
    m_rect.y = fragment.rect.y;
    m_rect.width = run.width;
    m_rect.height = run.height;
}

void RenderImage::finalize_inline_layout() {}

bool RenderImage::set_image(const ImageBitmap* image) {
    if (image == m_image) {
        return false;
    }
    m_image = image;
    return true;
}

bool RenderImage::should_inline() const {
    const auto* style = get_computed_style();
    auto display = style ? style->display : Css::ComputedStyle::Display::Inline;
    return display == Css::ComputedStyle::Display::Inline || display == Css::ComputedStyle::Display::InlineBlock;
}

}  // namespace Hummingbird::Layout
