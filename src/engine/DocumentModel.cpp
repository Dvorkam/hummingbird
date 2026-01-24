#include "engine/DocumentModel.h"

#include <ostream>
#include <utility>

#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "html/HtmlParser.h"
#include "layout/RenderObject.h"
#include "style/CssParser.h"
#include "style/Stylesheet.h"

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
    stylesheet_links_.clear();
    image_links_.clear();
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

void DocumentModel::apply_styles(const std::string& css) {
    const auto css_parse_start = Core::Clock::now();
    Css::Parser css_parser(css);
    auto stylesheet = css_parser.parse();
    const auto css_parse_end = Core::Clock::now();
    HB_LOG_INFO("[perf] css parse ms=" << Core::duration_ms(css_parse_start, css_parse_end)
                                       << " rules=" << stylesheet.rules.size());

    const auto style_start = Core::Clock::now();
    style_engine_.apply(stylesheet, dom_tree_.get());
    const auto style_end = Core::Clock::now();
    HB_LOG_INFO("[pipeline] applied stylesheet rules: " << stylesheet.rules.size());
    HB_LOG_INFO("[perf] style apply ms=" << Core::duration_ms(style_start, style_end));
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

size_t DocumentModel::render_tree_children() const {
    if (!render_tree_) return 0;
    return render_tree_->get_children().size();
}

}  // namespace Hummingbird::Engine
