#include "engine/document/DocumentModel.h"

#include <ostream>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "core/utils/Url.h"
#include "engine/document/DocumentLinkDiscovery.h"
#include "engine/document/FontFaceResolver.h"
#include "engine/document/FormSubmissionBuilder.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"
#include "style/compute/FontFaceRegistry.h"
#include "style/compute/Stylesheet.h"
#include "style/parser/CssParser.h"

namespace Hummingbird::Engine {

namespace {
size_t count_nodes_recursive(const DOM::Node* node) {
    if (!node) return 0;
    size_t total = 1;
    for (const auto& child : node->get_children()) {
        total += count_nodes_recursive(child.get());
    }
    return total;
}
}  // namespace

void DocumentModel::flush_compatibility_warnings(std::string_view document_url) {
    const size_t total = compatibility_warnings_.total_count();
    const size_t unique = compatibility_warnings_.unique_count();
    if (total > unique) {
        const std::string_view label = document_url.empty() ? std::string_view{"<unknown document>"} : document_url;
        HB_LOG_WARN("[compat-summary] " << label << ": " << total << " occurrences, " << unique << " unique, "
                                        << (total - unique) << " suppressed");

        constexpr size_t kSummaryLimit = 20;
        size_t emitted = 0;
        size_t repeated_entries = 0;
        for (const auto& entry : compatibility_warnings_.ranked()) {
            if (entry.count <= 1) {
                continue;
            }
            ++repeated_entries;
            if (emitted >= kSummaryLimit) {
                continue;
            }
            if (entry.category == Core::Utils::kUnsupportedFontFamilyWarning) {
                HB_LOG_WARN("[compat-summary] " << entry.count << "x [style] Unsupported font family list '"
                                                << entry.detail << "', falling back to Roboto");
            } else {
                HB_LOG_WARN("[compat-summary] " << entry.count << "x [" << entry.category << "] " << entry.detail);
            }
            ++emitted;
        }
        if (repeated_entries > emitted) {
            HB_LOG_WARN("[compat-summary] " << (repeated_entries - emitted) << " additional repeated warning(s)");
        }
    }
    compatibility_warnings_.clear();
}

void DocumentModel::reset() {
    dom_tree_.reset();
    render_tree_.reset();
    dom_arena_.reset();
    style_blocks_.clear();
    document_scripts_.clear();
    stylesheet_links_.clear();
    image_links_.clear();
    background_image_links_.clear();
    media_conditions_.clear();
    applied_media_ = {};
}

bool DocumentModel::media_conditions_change(const Css::MediaContext& media) const {
    for (const auto& condition : media_conditions_) {
        if (media_condition_matches(condition, applied_media_) != media_condition_matches(condition, media)) {
            return true;
        }
    }
    return false;
}

DocumentModel::ParseResult DocumentModel::parse_html(std::string_view html) {
    const auto parse_start = Core::Clock::now();
    Html::Parser::Result parse_result;
    Html::Parser parser(dom_arena_, html);
    parse_result = parser.parse();
    const auto parse_end = Core::Clock::now();

    dom_tree_ = std::move(parse_result.dom);
    style_blocks_ = std::move(parse_result.style_blocks);
    stylesheet_links_ = std::move(parse_result.stylesheet_links);
    image_links_ = std::move(parse_result.image_links);
    document_scripts_ = collect_document_scripts_from_dom(dom_tree_.get());

    if (!dom_tree_) {
        const bool arena_failed = dom_arena_.failed();
        if (arena_failed) {
            HB_LOG_ERROR("[pipeline] DOM arena budget exceeded, showing budget error page");
            if (load_budget_error_page()) {
                return {true, false};  // recovered with a user-facing error page
            }
        }
        HB_LOG_WARN("[pipeline] parsed empty DOM");
        return {false, arena_failed};
    }

    HB_LOG_INFO("[pipeline] parsed DOM children: " << dom_tree_->get_children().size()
                                                   << " total nodes: " << count_nodes_recursive(dom_tree_.get()));
    HB_LOG_INFO("[perf] html parse ms=" << Core::duration_ms(parse_start, parse_end));

    return {true, false};
}

namespace {
constexpr std::string_view kDomBudgetExceededPage = R"HTML(<!doctype html>
<html>
  <head><style>
    body { margin: 40px; color: #1d2433; background-color: #f4f6fb; font-family: sans-serif; }
    h1 { color: #14213d; }
  </style></head>
  <body>
    <h1>This page is too large to display</h1>
    <p>Hummingbird stopped loading this document because it exceeded the memory budget for a single page.</p>
    <p>Try opening a different page from the address bar.</p>
  </body>
</html>)HTML";

void mark_visited_recursive(DOM::Node* node, const std::unordered_set<std::string>& visited_urls,
                            std::string_view base_url) {
    if (auto* element = dynamic_cast<DOM::Element*>(node)) {
        if (element->get_tag_name() == Hummingbird::Html::TagNames::A) {
            const auto* href = element->find_attribute(Hummingbird::Html::AttributeNames::Href);
            bool is_visited = false;
            if (href && !href->empty()) {
                std::string resolved = Core::resolve_url(base_url, *href);
                if (resolved.empty()) {
                    resolved = *href;
                }
                is_visited = visited_urls.count(resolved) > 0;
            }
            element->set_pseudo_state(DOM::Element::PseudoState::Visited, is_visited);
        }
    }
    for (const auto& child : node->get_children()) {
        mark_visited_recursive(child.get(), visited_urls, base_url);
    }
}
}  // namespace

bool DocumentModel::load_budget_error_page() {
    // The over-budget parse left the arena in a failed state; reset frees the
    // partial DOM and clears the failure flag so the small error page fits.
    dom_arena_.reset();
    style_blocks_.clear();
    stylesheet_links_.clear();
    image_links_.clear();
    document_scripts_.clear();

    Html::Parser parser(dom_arena_, kDomBudgetExceededPage);
    auto parsed = parser.parse();
    dom_tree_ = std::move(parsed.dom);
    style_blocks_ = std::move(parsed.style_blocks);
    return static_cast<bool>(dom_tree_);
}

void DocumentModel::mark_visited_links(const std::unordered_set<std::string>& visited_urls, std::string_view base_url) {
    if (!dom_tree_ || visited_urls.empty()) {
        return;
    }
    mark_visited_recursive(dom_tree_.get(), visited_urls, base_url);
}

void DocumentModel::apply_styles(const std::string& css, const Css::MediaContext& media,
                                 const IFontFaceResolver* font_resolver) {
    const auto css_parse_start = Core::Clock::now();
    Css::Parser css_parser(css);
    auto stylesheet = css_parser.parse();
    const auto css_parse_end = Core::Clock::now();
    HB_LOG_INFO("[perf] css parse ms=" << Core::duration_ms(css_parse_start, css_parse_end)
                                       << " rules=" << stylesheet.rules.size());

    // Remember every media condition and the viewport it was evaluated
    // against, so a later resize can tell whether any rule would flip
    // (T-MEDIA-RESIZE-1).
    media_conditions_.clear();
    for (const auto& rule : stylesheet.rules) {
        if (rule.media) {
            media_conditions_.push_back(*rule.media);
        }
    }
    applied_media_ = media;

    // Resolve @font-face rules to loadable font keys so style compute can stamp
    // ComputedStyle::font_src; remote fonts still needing a fetch are exposed via
    // font_requests() (T-FONT-FACE-1).
    Css::FontFaceRegistry font_registry;
    font_requests_.clear();
    if (font_resolver && !stylesheet.font_faces.empty()) {
        font_registry = font_resolver->resolve_font_faces(stylesheet.font_faces, font_requests_);
    }

    const auto style_start = Core::Clock::now();
    style_engine_.apply(stylesheet, dom_tree_.get(), media, &font_registry);
    const auto style_end = Core::Clock::now();
    HB_LOG_INFO("[pipeline] applied stylesheet rules: " << stylesheet.rules.size());
    HB_LOG_INFO("[perf] style apply ms=" << Core::duration_ms(style_start, style_end));

    background_image_links_ = collect_background_image_links_from_dom(dom_tree_.get());
}

bool DocumentModel::build_render_tree() {
    const auto render_start = Core::Clock::now();
    render_tree_ = tree_builder_.build(dom_tree_.get());
    const auto render_end = Core::Clock::now();
    if (!render_tree_) {
        HB_LOG_WARN("[pipeline] render tree build skipped");
        return false;
    }
    HB_LOG_INFO("[perf] render tree build ms=" << Core::duration_ms(render_start, render_end));
    return true;
}

std::optional<FormSubmission> DocumentModel::build_form_submission(const DOM::Element& input,
                                                                   std::string_view base_url) const {
    return build_form_submission_from_dom(dom_tree_.get(), input, base_url);
}

size_t DocumentModel::render_tree_children() const {
    if (!render_tree_) return 0;
    return render_tree_->get_children().size();
}

}  // namespace Hummingbird::Engine
