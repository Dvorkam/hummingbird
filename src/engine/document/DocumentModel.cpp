#include "engine/document/DocumentModel.h"

#include <ostream>
#include <unordered_set>
#include <utility>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"
#include "core/utils/Timing.h"
#include "core/utils/Url.h"
#include "core/utils/UrlEncoding.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"
#include "style/parser/CssParser.h"
#include "style/compute/Stylesheet.h"

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

const DOM::Element* find_enclosing_form(const DOM::Node* node) {
    const DOM::Node* current = node;
    while (current) {
        auto* element = dynamic_cast<const DOM::Element*>(current);
        if (element && element->get_tag_name() == Hummingbird::Html::TagNames::Form) {
            return element;
        }
        current = current->get_parent();
    }
    return nullptr;
}

struct FormField {
    std::string_view name;
    std::string_view value;
};

const DOM::Element* find_form_by_id(const DOM::Node* node, std::string_view id) {
    auto* element = dynamic_cast<const DOM::Element*>(node);
    if (element && element->get_tag_name() == Hummingbird::Html::TagNames::Form) {
        if (const auto* attr = element->find_attribute(Hummingbird::Html::AttributeNames::Id)) {
            if (*attr == id) {
                return element;
            }
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* match = find_form_by_id(child.get(), id)) {
            return match;
        }
    }
    return nullptr;
}

void maybe_add_input_field(const DOM::Element& element, std::unordered_set<const DOM::Element*>& visited,
                           std::vector<FormField>& fields) {
    if (element.get_tag_name() != Hummingbird::Html::TagNames::Input) {
        return;
    }
    if (!visited.insert(&element).second) {
        return;
    }
    const auto* name = element.find_attribute(Hummingbird::Html::AttributeNames::Name);
    if (!name || name->empty()) {
        return;
    }
    std::string_view value;
    if (const auto* attr_value = element.find_attribute(Hummingbird::Html::AttributeNames::Value)) {
        value = *attr_value;
    }
    fields.push_back({*name, value});
}

void collect_form_inputs(const DOM::Node* node, std::unordered_set<const DOM::Element*>& visited,
                         std::vector<FormField>& fields) {
    if (auto* element = dynamic_cast<const DOM::Element*>(node)) {
        maybe_add_input_field(*element, visited, fields);
    }

    for (const auto& child : node->get_children()) {
        collect_form_inputs(child.get(), visited, fields);
    }
}

void collect_associated_inputs(const DOM::Node* node, std::string_view form_id,
                               std::unordered_set<const DOM::Element*>& visited, std::vector<FormField>& fields) {
    if (auto* element = dynamic_cast<const DOM::Element*>(node)) {
        if (const auto* form_attr = element->find_attribute(Hummingbird::Html::AttributeNames::Form)) {
            if (*form_attr == form_id) {
                maybe_add_input_field(*element, visited, fields);
            }
        }
    }

    for (const auto& child : node->get_children()) {
        collect_associated_inputs(child.get(), form_id, visited, fields);
    }
}

std::string append_query(std::string base, std::string_view query) {
    if (query.empty()) {
        return base;
    }
    const bool has_query = base.find('?') != std::string::npos;
    if (!has_query) {
        base.push_back('?');
        base.append(query);
        return base;
    }
    if (!base.empty() && base.back() != '?' && base.back() != '&') {
        base.push_back('&');
    }
    base.append(query);
    return base;
}

void collect_script_text(const DOM::Node* node, std::string& out) {
    if (!node) return;
    if (auto* text_node = dynamic_cast<const DOM::Text*>(node)) {
        out.append(text_node->get_text());
        return;
    }
    for (const auto& child : node->get_children()) {
        collect_script_text(child.get(), out);
    }
}

void collect_script_blocks(const DOM::Node* node, std::vector<std::string>& scripts) {
    if (!node) return;
    if (auto* element = dynamic_cast<const DOM::Element*>(node)) {
        if (element->get_tag_name() == Hummingbird::Html::TagNames::Script) {
            std::string text;
            for (const auto& child : element->get_children()) {
                collect_script_text(child.get(), text);
            }
            if (!text.empty()) {
                scripts.push_back(std::move(text));
            }
            return;
        }
    }
    for (const auto& child : node->get_children()) {
        collect_script_blocks(child.get(), scripts);
    }
}

void collect_background_image_links(const DOM::Node* node, std::vector<std::string>& links,
                                    std::unordered_set<std::string>& seen) {
    if (!node) return;
    auto style = node->get_computed_style();
    if (style && style->background_image && !style->background_image->empty()) {
        if (seen.insert(*style->background_image).second) {
            links.push_back(*style->background_image);
        }
    }
    for (const auto& child : node->get_children()) {
        collect_background_image_links(child.get(), links, seen);
    }
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
    script_blocks_.clear();
    if (dom_tree_) {
        collect_script_blocks(dom_tree_.get(), script_blocks_);
    }

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

    background_image_links_.clear();
    std::unordered_set<std::string> seen;
    collect_background_image_links(dom_tree_.get(), background_image_links_, seen);
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

std::optional<std::string> DocumentModel::build_form_submission_url(const DOM::Element& input,
                                                                    std::string_view base_url) const {
    if (base_url.empty()) {
        return std::nullopt;
    }

    const DOM::Element* form = nullptr;
    std::string_view form_id;
    if (const auto* form_attr = input.find_attribute(Hummingbird::Html::AttributeNames::Form)) {
        if (!form_attr->empty()) {
            form_id = *form_attr;
            if (dom_tree_) {
                form = find_form_by_id(dom_tree_.get(), form_id);
            }
            if (!form) {
                HB_LOG_WARN("[form] form id not found: " << form_id);
                return std::nullopt;
            }
        }
    }
    if (!form) {
        form = find_enclosing_form(&input);
    }
    if (!form) {
        return std::nullopt;
    }

    if (const auto* method = form->find_attribute(Hummingbird::Html::AttributeNames::Method)) {
        if (!Core::Utils::equals_ignore_case(*method, "get")) {
            HB_LOG_WARN("[form] unsupported method: " << *method);
            return std::nullopt;
        }
    }

    std::string action;
    if (const auto* action_attr = form->find_attribute(Hummingbird::Html::AttributeNames::Action)) {
        action = *action_attr;
    }
    std::string resolved_action = action.empty() ? std::string(base_url) : Core::resolve_url(base_url, action);
    if (resolved_action.empty()) {
        return std::nullopt;
    }

    std::vector<FormField> fields;
    std::unordered_set<const DOM::Element*> visited;
    collect_form_inputs(form, visited, fields);

    if (!form_id.empty()) {
        if (dom_tree_) {
            collect_associated_inputs(dom_tree_.get(), form_id, visited, fields);
        }
    } else if (const auto* id_attr = form->find_attribute(Hummingbird::Html::AttributeNames::Id)) {
        if (!id_attr->empty() && dom_tree_) {
            collect_associated_inputs(dom_tree_.get(), *id_attr, visited, fields);
        }
    }

    std::string query;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            query.push_back('&');
        }
        query.append(Core::Utils::url_encode_component(fields[i].name));
        query.push_back('=');
        query.append(Core::Utils::url_encode_component(fields[i].value));
    }

    return append_query(std::move(resolved_action), query);
}

size_t DocumentModel::render_tree_children() const {
    if (!render_tree_) return 0;
    return render_tree_->get_children().size();
}

}  // namespace Hummingbird::Engine
