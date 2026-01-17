#include "layout/RenderImage.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "core/dom/ElementUtils.h"
#include "core/platform_api/IImageDecoder.h"
#include "core/utils/AssetPath.h"
#include "core/utils/ParseUtils.h"
#include "html/HtmlAttributeNames.h"
#include "layout/LayoutMetricsUtils.h"
#include "layout/PaintUtils.h"
#include "layout/inline/InlineTypes.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kDefaultImageWidth = 300.0f;
constexpr float kDefaultImageHeight = 150.0f;
constexpr float kAltTextPadding = 4.0f;

const Color kPlaceholderFill{230, 230, 230, 255};
const Color kPlaceholderStroke{150, 150, 150, 255};

struct LayoutSize {
    float width;
    float height;
};

std::optional<float> find_attribute_dimension(const DOM::Element& element, std::string_view name) {
    if (const auto value = DOM::find_attribute_value(element, name)) {
        return Core::Utils::parse_float(*value, Core::Utils::NumberParseMode::AllowTrailing);
    }
    return std::nullopt;
}

float resolve_width(const DOM::Element& element, const Css::ComputedStyle* style, const ImageBitmap* image) {
    if (style && style->width.has_value()) {
        return std::max(0.0f, *style->width);
    }
    if (auto attr = find_attribute_dimension(element, Hummingbird::Html::AttributeNames::Width)) {
        return std::max(0.0f, *attr);
    }
    if (image && image->width > 0) {
        return static_cast<float>(image->width);
    }
    return kDefaultImageWidth;
}

float resolve_height(const DOM::Element& element, const Css::ComputedStyle* style, const ImageBitmap* image) {
    if (style && style->height.has_value()) {
        return std::max(0.0f, *style->height);
    }
    if (auto attr = find_attribute_dimension(element, Hummingbird::Html::AttributeNames::Height)) {
        return std::max(0.0f, *attr);
    }
    if (image && image->height > 0) {
        return static_cast<float>(image->height);
    }
    return kDefaultImageHeight;
}

LayoutSize compute_layout_size(const DOM::Element& element, const Css::ComputedStyle* style, const ImageBitmap* image) {
    Metrics::Insets insets = Metrics::compute_insets(style);
    float content_width = resolve_width(element, style, image);
    float content_height = resolve_height(element, style, image);
    return {content_width + insets.left + insets.right, content_height + insets.top + insets.bottom};
}

const std::string& resolve_default_font_path() {
    static const std::string kDefaultFontPath =
        Hummingbird::resolve_asset_path_string("assets/fonts/Roboto-Regular.ttf");
    return kDefaultFontPath;
}
}  // namespace

void RenderImage::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    auto* element = static_cast<const DOM::Element*>(get_dom_node());
    const auto* style = get_computed_style();
    LayoutSize size = compute_layout_size(*element, style, m_image);

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
        context.draw_image(*m_image, content);
        return;
    }

    bool has_background = style && style->background.has_value();
    if (!has_background) {
        context.fill_rect(content, kPlaceholderFill);
    }
    PaintUtils::draw_outline(context, content, kPlaceholderStroke);

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
    LayoutSize size = compute_layout_size(*element, style, m_image);
    m_inline_measured_width = size.width;
    m_inline_measured_height = size.height;
}

void RenderImage::collect_inline_runs(IGraphicsContext& /*context*/, std::vector<InlineRun>& runs) {
    InlineRun run;
    run.owner = this;
    run.local_index = 0;
    run.width = m_inline_measured_width;
    run.height = m_inline_measured_height;
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
