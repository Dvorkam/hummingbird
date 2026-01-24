#include "engine/script/DocumentScriptHost.h"

#include <utility>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "html/HtmlAttributeNames.h"

namespace Hummingbird::Engine {

namespace {
DOM::Element* find_element_by_id(DOM::Node* node, std::string_view id) {
    if (!node) return nullptr;

    if (auto* element = dynamic_cast<DOM::Element*>(node)) {
        if (const auto* attr = element->find_attribute(Hummingbird::Html::AttributeNames::Id)) {
            if (*attr == id) {
                return element;
            }
        }
    }

    for (const auto& child : node->get_children()) {
        if (auto* match = find_element_by_id(child.get(), id)) {
            return match;
        }
    }
    return nullptr;
}

void collect_text(const DOM::Node* node, std::string& out) {
    if (!node) return;
    if (auto* text_node = dynamic_cast<const DOM::Text*>(node)) {
        out.append(text_node->get_text());
        return;
    }
    for (const auto& child : node->get_children()) {
        collect_text(child.get(), out);
    }
}
}  // namespace

void DocumentScriptHost::reset(DOM::Node* root, Core::ArenaAllocator* arena) {
    root_ = root;
    arena_ = arena;
    mutated_ = false;
}

void DocumentScriptHost::clear() {
    root_ = nullptr;
    arena_ = nullptr;
    mutated_ = false;
}

bool DocumentScriptHost::consume_mutations() {
    bool result = mutated_;
    mutated_ = false;
    return result;
}

DOM::Element* DocumentScriptHost::get_element_by_id(std::string_view id) {
    if (!root_ || id.empty()) {
        return nullptr;
    }
    return find_element_by_id(root_, id);
}

std::string DocumentScriptHost::get_text_content(const DOM::Element* element) {
    if (!element) return {};
    std::string text;
    collect_text(element, text);
    return text;
}

void DocumentScriptHost::set_text_content(DOM::Element* element, std::string_view text) {
    if (!element || !arena_) return;
    element->clear_children();
    if (!text.empty()) {
        element->append_child(DOM::Text::create(*arena_, text));
    }
    mutated_ = true;
}

void DocumentScriptHost::set_attribute(DOM::Element* element, std::string_view name, std::string_view value) {
    if (!element) return;
    element->set_attribute(name, value);
    mutated_ = true;
}

}  // namespace Hummingbird::Engine
