#include "style/SelectorMatcher.h"

#include <sstream>
#include <string_view>

#include "core/dom/Element.h"

namespace Hummingbird::Css {

namespace {
const std::string* find_attribute_value(const DOM::Element& element, std::string_view key) {
    const auto& attrs = element.get_attributes();
    auto it = attrs.find(std::string(key));
    if (it == attrs.end()) return nullptr;
    return &it->second;
}

bool has_class(const DOM::Element& element, const std::string& expected) {
    const auto* value = find_attribute_value(element, "class");
    if (!value) return false;
    std::istringstream ss(*value);
    std::string cls;
    while (ss >> cls) {
        if (cls == expected) return true;
    }
    return false;
}

bool has_id(const DOM::Element& element, const std::string& expected) {
    const auto* value = find_attribute_value(element, "id");
    return value && *value == expected;
}
}  // namespace

bool matches_selector(const DOM::Node* node, const Selector& selector) {
    auto element = dynamic_cast<const DOM::Element*>(node);
    if (!element) {
        return false;
    }

    switch (selector.type) {
        case SelectorType::Tag:
            return element->get_tag_name() == selector.value;
        case SelectorType::Class: {
            return has_class(*element, selector.value);
        }
        case SelectorType::Id: {
            return has_id(*element, selector.value);
        }
    }
    return false;
}

}  // namespace Hummingbird::Css
