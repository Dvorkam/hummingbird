#include "style/SelectorMatcher.h"
#include "core/dom/Element.h"
#include <sstream>

namespace Hummingbird::Css {

bool matches_selector(const DOM::Node* node, const Selector& selector) {
    auto element = dynamic_cast<const DOM::Element*>(node);
    if (!element) {
        return false;
    }

    switch (selector.type) {
        case SelectorType::Tag:
            return element->get_tag_name() == selector.value;
        case SelectorType::Class: {
            auto& attrs = element->get_attributes();
            auto it = attrs.find("class");
            if (it == attrs.end()) return false;
            std::istringstream ss(it->second);
            std::string cls;
            while (ss >> cls) {
                if (cls == selector.value) return true;
            }
            return false;
        }
        case SelectorType::Id: {
            auto& attrs = element->get_attributes();
            auto it = attrs.find("id");
            return it != attrs.end() && it->second == selector.value;
        }
    }
    return false;
}

} // namespace Hummingbird::Css
