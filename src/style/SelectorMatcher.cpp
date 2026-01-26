#include "style/SelectorMatcher.h"

#include <stddef.h>

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "html/HtmlAttributeNames.h"
#include "style/Stylesheet.h"

namespace Hummingbird::Css {

namespace {
bool has_id(const DOM::Element& element, const std::string& expected) {
    const auto* value = element.find_attribute(Hummingbird::Html::AttributeNames::Id);
    return value && *value == expected;
}

std::vector<std::string_view> split_classes(std::string_view class_list) {
    std::vector<std::string_view> tokens;
    size_t i = 0;
    while (i < class_list.size()) {
        while (i < class_list.size() && std::isspace(static_cast<unsigned char>(class_list[i])) != 0) {
            ++i;
        }
        if (i >= class_list.size()) {
            break;
        }
        size_t start = i;
        while (i < class_list.size() && std::isspace(static_cast<unsigned char>(class_list[i])) == 0) {
            ++i;
        }
        tokens.emplace_back(class_list.substr(start, i - start));
    }
    return tokens;
}

bool has_all_classes(const DOM::Element& element, const std::vector<std::string>& expected) {
    if (expected.empty()) {
        return true;
    }
    const auto* value = element.find_attribute(Hummingbird::Html::AttributeNames::Class);
    if (!value || value->empty()) {
        return false;
    }
    auto tokens = split_classes(*value);
    for (const auto& cls : expected) {
        bool found = false;
        for (auto token : tokens) {
            if (token == cls) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}
}  // namespace

bool matches_simple_selector(const DOM::Node* node, const SelectorPart& selector) {
    auto element = dynamic_cast<const DOM::Element*>(node);
    if (!element) {
        return false;
    }

    if (!selector.tag.empty() && selector.tag != "*" && element->get_tag_name() != selector.tag) {
        return false;
    }
    if (!selector.id.empty() && !has_id(*element, selector.id)) {
        return false;
    }
    return has_all_classes(*element, selector.classes);
}

bool matches_selector(const DOM::Node* node, const Selector& selector) {
    if (selector.parts.empty()) {
        return false;
    }
    const DOM::Node* current = node;
    if (!matches_simple_selector(current, selector.parts.back())) {
        return false;
    }
    if (selector.parts.size() == 1) {
        return true;
    }
    for (size_t i = selector.parts.size() - 1; i-- > 0;) {
        const auto& part = selector.parts[i];
        const DOM::Node* cursor = current ? current->get_parent() : nullptr;
        bool found = false;
        while (cursor) {
            if (matches_simple_selector(cursor, part)) {
                found = true;
                current = cursor;
                break;
            }
            cursor = cursor->get_parent();
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

}  // namespace Hummingbird::Css
