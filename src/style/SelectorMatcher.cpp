#include "style/SelectorMatcher.h"

#include <cctype>
#include <string_view>
#include <vector>

#include "core/dom/Element.h"
#include "html/HtmlAttributeNames.h"

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
    return has_all_classes(*element, selector.classes);
}

}  // namespace Hummingbird::Css
