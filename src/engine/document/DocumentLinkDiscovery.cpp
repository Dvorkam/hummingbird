#include "engine/document/DocumentLinkDiscovery.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Engine {

namespace {

void collect_script_text_recursive(const DOM::Node* node, std::string& out) {
    if (!node) return;
    if (auto* text_node = dynamic_cast<const DOM::Text*>(node)) {
        out.append(text_node->get_text());
        return;
    }
    for (const auto& child : node->get_children()) {
        collect_script_text_recursive(child.get(), out);
    }
}

// Only classic JS runs: a missing/empty type attribute or a JavaScript MIME
// type. Data blocks (application/json, text/template, ...) and modules are
// skipped — module semantics (import/export) would throw under classic eval.
bool is_runnable_script_type(const DOM::Element& element) {
    const auto* type = element.find_attribute(Hummingbird::Html::AttributeNames::Type);
    if (!type || type->empty()) return true;
    // Attribute values (unlike names) aren't lower-cased on parse, so a MIME
    // type like "text/JavaScript" must be compared case-insensitively.
    std::string lower = Core::Utils::to_lower(*type);
    return lower.find("javascript") != std::string::npos || lower == "text/js";
}

void collect_document_scripts_recursive(const DOM::Node* node, std::vector<DocumentScriptRef>& scripts) {
    if (!node) return;
    if (auto* element = dynamic_cast<const DOM::Element*>(node)) {
        if (element->get_tag_name() == Hummingbird::Html::TagNames::Script) {
            if (!is_runnable_script_type(*element)) {
                return;
            }
            const auto* src = element->find_attribute(Hummingbird::Html::AttributeNames::Src);
            if (src && !src->empty()) {
                scripts.push_back({*src, {}});
                return;
            }
            std::string text;
            for (const auto& child : element->get_children()) {
                collect_script_text_recursive(child.get(), text);
            }
            if (!text.empty()) {
                scripts.push_back({{}, std::move(text)});
            }
            return;
        }
    }
    for (const auto& child : node->get_children()) {
        collect_document_scripts_recursive(child.get(), scripts);
    }
}

void collect_background_links_recursive(const DOM::Node* node, std::vector<std::string>& links,
                                        std::unordered_set<std::string>& seen) {
    if (!node) return;
    auto style = node->get_computed_style();
    if (style && style->background_image && !style->background_image->empty()) {
        if (seen.insert(*style->background_image).second) {
            links.push_back(*style->background_image);
        }
    }
    for (const auto& child : node->get_children()) {
        collect_background_links_recursive(child.get(), links, seen);
    }
}

}  // namespace

std::vector<DocumentScriptRef> collect_document_scripts_from_dom(const DOM::Node* root) {
    std::vector<DocumentScriptRef> scripts;
    collect_document_scripts_recursive(root, scripts);
    return scripts;
}

std::vector<std::string> collect_background_image_links_from_dom(const DOM::Node* root) {
    std::vector<std::string> links;
    std::unordered_set<std::string> seen;
    collect_background_links_recursive(root, links, seen);
    return links;
}

}  // namespace Hummingbird::Engine
