#include "style/selector/SelectorMatcher.h"

#include <stddef.h>

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "style/compute/Stylesheet.h"

namespace Hummingbird::Css {

namespace {
bool has_id(const DOM::Element& element, const std::string& expected) {
    const auto* value = element.find_attribute(Hummingbird::Html::AttributeNames::Id);
    return value && *value == expected;
}

bool has_all_classes(const DOM::Element& element, const std::vector<std::string>& expected) {
    if (expected.empty()) {
        return true;
    }
    const auto* value = element.find_attribute(Hummingbird::Html::AttributeNames::Class);
    if (!value || value->empty()) {
        return false;
    }
    auto tokens = Core::Utils::split_ascii_whitespace(*value);
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

// Element siblings only: text/comment nodes between elements are ignored,
// matching how CSS sibling combinators are defined.
const DOM::Node* previous_element_sibling(const DOM::Node* node) {
    const DOM::Node* parent = node ? node->get_parent() : nullptr;
    if (!parent) {
        return nullptr;
    }
    const DOM::Node* previous = nullptr;
    for (const auto& child : parent->get_children()) {
        if (child.get() == node) {
            return previous;
        }
        if (dynamic_cast<const DOM::Element*>(child.get())) {
            previous = child.get();
        }
    }
    return nullptr;
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
    if (!has_all_classes(*element, selector.classes)) {
        return false;
    }

    for (auto pseudo : selector.pseudo_classes) {
        bool matches = false;
        switch (pseudo) {
            case SelectorPart::PseudoClass::Hover:
                matches = element->has_pseudo_state(DOM::Element::PseudoState::Hover);
                break;
            case SelectorPart::PseudoClass::Active:
                matches = element->has_pseudo_state(DOM::Element::PseudoState::Active);
                break;
            case SelectorPart::PseudoClass::Focus:
                matches = element->has_pseudo_state(DOM::Element::PseudoState::Focus);
                break;
        }
        if (!matches) {
            return false;
        }
    }

    return true;
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

    auto combinator_for = [&](size_t parent_index) {
        if (parent_index < selector.combinators.size()) {
            return selector.combinators[parent_index];
        }
        return Selector::Combinator::Descendant;
    };

    for (size_t i = selector.parts.size() - 1; i-- > 0;) {
        const auto& part = selector.parts[i];
        const auto combinator = combinator_for(i);
        if (combinator == Selector::Combinator::Child) {
            const DOM::Node* parent = current ? current->get_parent() : nullptr;
            if (!parent || !matches_simple_selector(parent, part)) {
                return false;
            }
            current = parent;
            continue;
        }

        if (combinator == Selector::Combinator::NextSibling) {
            const DOM::Node* sibling = previous_element_sibling(current);
            if (!sibling || !matches_simple_selector(sibling, part)) {
                return false;
            }
            current = sibling;
            continue;
        }

        if (combinator == Selector::Combinator::SubsequentSibling) {
            const DOM::Node* sibling = previous_element_sibling(current);
            bool sibling_found = false;
            while (sibling) {
                if (matches_simple_selector(sibling, part)) {
                    sibling_found = true;
                    current = sibling;
                    break;
                }
                sibling = previous_element_sibling(sibling);
            }
            if (!sibling_found) {
                return false;
            }
            continue;
        }

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
