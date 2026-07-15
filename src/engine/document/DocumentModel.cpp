#include "engine/document/DocumentModel.h"

#include <ostream>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "core/utils/Url.h"
#include "engine/document/DocumentLinkDiscovery.h"
#include "engine/document/FormSubmissionBuilder.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"
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

void DocumentModel::reset() {
    dom_tree_.reset();
    render_tree_.reset();
    dom_arena_.reset();
    style_blocks_.clear();
    script_blocks_.clear();
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
    script_blocks_ = collect_script_blocks_from_dom(dom_tree_.get());

    if (!dom_tree_) {
        const bool arena_failed = dom_arena_.failed();
        if (arena_failed) {
            HB_LOG_ERROR("[pipeline] DOM arena budget exceeded, resetting document");
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

void DocumentModel::mark_visited_links(const std::unordered_set<std::string>& visited_urls, std::string_view base_url) {
    if (!dom_tree_ || visited_urls.empty()) {
        return;
    }
    mark_visited_recursive(dom_tree_.get(), visited_urls, base_url);
}

void DocumentModel::apply_styles(const std::string& css, const Css::MediaContext& media) {
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

    const auto style_start = Core::Clock::now();
    style_engine_.apply(stylesheet, dom_tree_.get(), media);
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
