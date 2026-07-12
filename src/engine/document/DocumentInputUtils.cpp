#include "engine/document/DocumentInputUtils.h"

#include "core/dom/Element.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"

namespace Hummingbird::Engine {

bool is_input_element(const DOM::Element* element) {
    return element && element->get_tag_name() == Hummingbird::Html::TagNames::Input;
}

bool is_button_element(const DOM::Element* element) {
    return element && element->get_tag_name() == Hummingbird::Html::TagNames::Button;
}

bool is_interactive_control_element(const DOM::Element* element) {
    return is_input_element(element) || is_button_element(element);
}

bool is_editable_input_element(const DOM::Element* element) {
    if (!is_input_element(element)) {
        return false;
    }

    const auto* type = element->find_attribute(Hummingbird::Html::AttributeNames::Type);
    if (!type || type->empty()) {
        return true;  // default <input> type is text
    }

    return !Core::Utils::equals_ignore_case(*type, "button") && !Core::Utils::equals_ignore_case(*type, "submit") &&
           !Core::Utils::equals_ignore_case(*type, "reset") && !Core::Utils::equals_ignore_case(*type, "checkbox") &&
           !Core::Utils::equals_ignore_case(*type, "radio") && !Core::Utils::equals_ignore_case(*type, "file") &&
           !Core::Utils::equals_ignore_case(*type, "hidden") && !Core::Utils::equals_ignore_case(*type, "image");
}

bool is_autofocus_input_element(const DOM::Element* element) {
    if (!is_editable_input_element(element)) {
        return false;
    }
    return element->find_attribute(Hummingbird::Html::AttributeNames::Autofocus) != nullptr;
}

std::string input_value(const DOM::Element& element) {
    if (const auto* value = element.find_attribute(Hummingbird::Html::AttributeNames::Value)) {
        return *value;
    }
    return {};
}

std::string describe_input_target(const DOM::Element* element) {
    if (!element) {
        return "<none>";
    }
    std::string desc = "<" + element->get_tag_name() + ">";
    if (const auto* id = element->find_attribute(Hummingbird::Html::AttributeNames::Id); id && !id->empty()) {
        desc += "#" + *id;
    }
    if (const auto* cls = element->find_attribute(Hummingbird::Html::AttributeNames::Class); cls && !cls->empty()) {
        desc += "." + *cls;
    }
    if (const auto* type = element->find_attribute(Hummingbird::Html::AttributeNames::Type); type && !type->empty()) {
        desc += "[type=" + *type + "]";
    }
    return desc;
}

void set_input_value(DOM::Element& element, std::string_view value) {
    element.set_attribute(Hummingbird::Html::AttributeNames::Value, value);
}

}  // namespace Hummingbird::Engine
