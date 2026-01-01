#include "layout/RenderImage.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include "layout/inline/InlineTypes.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kDefaultImageWidth = 300.0f;
constexpr float kDefaultImageHeight = 150.0f;

struct Insets {
    float left;
    float right;
    float top;
    float bottom;
};

struct LayoutSize {
    float width;
    float height;
};

Insets compute_insets(const Css::ComputedStyle* style) {
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float border_top = style ? style->border_width.top : 0.0f;
    float border_bottom = style ? style->border_width.bottom : 0.0f;
    return {padding_left + border_left, padding_right + border_right, padding_top + border_top,
            padding_bottom + border_bottom};
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::optional<float> parse_dimension(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    std::string temp(value);
    char* end = nullptr;
    float parsed = std::strtof(temp.c_str(), &end);
    if (end == temp.c_str()) {
        return std::nullopt;
    }
    if (parsed < 0.0f) {
        parsed = 0.0f;
    }
    return parsed;
}

std::optional<float> find_attribute_dimension(const DOM::Element& element, std::string_view name) {
    for (const auto& [key, value] : element.get_attributes()) {
        if (iequals(key, name)) {
            return parse_dimension(value);
        }
    }
    return std::nullopt;
}

float resolve_width(const DOM::Element& element, const Css::ComputedStyle* style) {
    if (style && style->width.has_value()) {
        return std::max(0.0f, *style->width);
    }
    if (auto attr = find_attribute_dimension(element, "width")) {
        return std::max(0.0f, *attr);
    }
    return kDefaultImageWidth;
}

float resolve_height(const DOM::Element& element, const Css::ComputedStyle* style) {
    if (style && style->height.has_value()) {
        return std::max(0.0f, *style->height);
    }
    if (auto attr = find_attribute_dimension(element, "height")) {
        return std::max(0.0f, *attr);
    }
    return kDefaultImageHeight;
}

LayoutSize compute_layout_size(const DOM::Element& element, const Css::ComputedStyle* style) {
    Insets insets = compute_insets(style);
    float content_width = resolve_width(element, style);
    float content_height = resolve_height(element, style);
    return {content_width + insets.left + insets.right, content_height + insets.top + insets.bottom};
}
}  // namespace

void RenderImage::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    auto* element = static_cast<const DOM::Element*>(get_dom_node());
    const auto* style = get_computed_style();
    LayoutSize size = compute_layout_size(*element, style);

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = size.width;
    m_rect.height = size.height;
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
    LayoutSize size = compute_layout_size(*element, style);
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

bool RenderImage::should_inline() const {
    const auto* style = get_computed_style();
    auto display = style ? style->display : Css::ComputedStyle::Display::Inline;
    return display == Css::ComputedStyle::Display::Inline || display == Css::ComputedStyle::Display::InlineBlock;
}

}  // namespace Hummingbird::Layout
