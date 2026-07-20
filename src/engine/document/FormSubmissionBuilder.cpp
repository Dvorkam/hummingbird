#include "engine/document/FormSubmissionBuilder.h"

#include <ostream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"
#include "core/utils/Url.h"
#include "core/utils/UrlEncoding.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"

namespace Hummingbird::Engine {

namespace {

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

void maybe_add_form_field(const DOM::Element& element, std::unordered_set<const DOM::Element*>& visited,
                          std::vector<FormField>& fields) {
    const auto& tag = element.get_tag_name();
    if (tag != Hummingbird::Html::TagNames::Input && tag != Hummingbird::Html::TagNames::Textarea) {
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
        maybe_add_form_field(*element, visited, fields);
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
                maybe_add_form_field(*element, visited, fields);
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

FormSubmitMethod parse_form_method(const DOM::Element& form) {
    const auto* method = form.find_attribute(Hummingbird::Html::AttributeNames::Method);
    if (!method || method->empty() || Core::Utils::equals_ignore_case(*method, "get")) {
        return FormSubmitMethod::Get;
    }
    if (Core::Utils::equals_ignore_case(*method, "post")) {
        return FormSubmitMethod::Post;
    }
    HB_LOG_WARN("[form] unsupported method: " << *method << ", falling back to GET");
    return FormSubmitMethod::Get;
}

}  // namespace

std::optional<FormSubmission> build_form_submission_from_dom(const DOM::Node* dom_tree, const DOM::Element& input,
                                                             std::string_view base_url) {
    if (base_url.empty()) {
        return std::nullopt;
    }

    const DOM::Element* form = nullptr;
    std::string_view form_id;
    if (const auto* form_attr = input.find_attribute(Hummingbird::Html::AttributeNames::Form)) {
        if (!form_attr->empty()) {
            form_id = *form_attr;
            if (dom_tree) {
                form = find_form_by_id(dom_tree, form_id);
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

    const FormSubmitMethod form_method = parse_form_method(*form);

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
        if (dom_tree) {
            collect_associated_inputs(dom_tree, form_id, visited, fields);
        }
    } else if (const auto* id_attr = form->find_attribute(Hummingbird::Html::AttributeNames::Id)) {
        if (!id_attr->empty() && dom_tree) {
            collect_associated_inputs(dom_tree, *id_attr, visited, fields);
        }
    }

    std::string encoded_fields;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            encoded_fields.push_back('&');
        }
        encoded_fields.append(Core::Utils::form_url_encode_component(fields[i].name));
        encoded_fields.push_back('=');
        encoded_fields.append(Core::Utils::form_url_encode_component(fields[i].value));
    }

    FormSubmission submission;
    submission.method = form_method;
    submission.content_type = "application/x-www-form-urlencoded";
    submission.form_element = form;  // target of the DOM `submit` event (7.2.4.4)
    if (form_method == FormSubmitMethod::Post) {
        submission.url = std::move(resolved_action);
        submission.body = std::move(encoded_fields);
        return submission;
    }

    submission.url = append_query(std::move(resolved_action), encoded_fields);
    return submission;
}

}  // namespace Hummingbird::Engine
