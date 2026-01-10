#include "style/SelectorMatcher.h"

#include <sstream>
#include <string_view>

#include "core/dom/Element.h"
#include "html/HtmlAttributeNames.h"

namespace Hummingbird::Css {

namespace {
const std::string* find_attribute_value(const DOM::Element& element, std::string_view key) {
    const auto& attrs = element.get_attributes();
    auto it = attrs.find(std::string(key));
    if (it == attrs.end()) return nullptr;
    return &it->second;
}

bool has_class(const DOM::Element& element, const std::string& expected) {
    const auto* value = find_attribute_value(element, Hummingbird::Html::AttributeNames::Class);
    if (!value) return false;
    std::istringstream ss(*value);
    std::string cls;
    while (ss >> cls) {
        if (cls == expected) return true;
    }
    return false;
}

bool has_id(const DOM::Element& element, const std::string& expected) {
    const auto* value = find_attribute_value(element, Hummingbird::Html::AttributeNames::Id);
    return value && *value == expected;
}
}  // namespace

bool matches_selector(const DOM::Node* node, const Selector& selector) {
    auto element = dynamic_cast<const DOM::Element*>(node);
    if (!element) {
        return false;
    }

    if (!selector.tag.empty() && element->get_tag_name() != selector.tag) {
        return false;
    }
    if (!selector.id.empty() && !has_id(*element, selector.id)) {
        return false;
    }
    for (const auto& cls : selector.classes) {
        if (!has_class(*element, cls)) {
            return false;
        }
    }
    return true;
}

}  // namespace Hummingbird::Css
