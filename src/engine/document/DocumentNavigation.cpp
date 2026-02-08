#include "engine/document/DocumentNavigation.h"

#include "core/dom/Element.h"
#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"
#include "engine/document/DocumentModel.h"
#include "engine/document/HitTestUtils.h"
#include "engine/resources/ResourceUrl.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Engine {

namespace {
std::optional<std::string> resolve_anchor_href(const DOM::Node* node, std::string_view base_url) {
    const DOM::Node* current = node;
    while (current) {
        auto* element = dynamic_cast<const DOM::Element*>(current);
        if (element && element->get_tag_name() == Hummingbird::Html::TagNames::A) {
            const auto* href = element->find_attribute(Hummingbird::Html::AttributeNames::Href);
            if (!href || href->empty()) {
                return std::nullopt;
            }
            auto resolved = resolve_resource_url(base_url, *href);
            return resolved.resolved.empty() ? std::optional<std::string>(*href)
                                             : std::optional<std::string>(std::move(resolved.resolved));
        }
        current = current->get_parent();
    }
    return std::nullopt;
}

const DOM::Element* resolve_submit_element(const DOM::Node* node) {
    const DOM::Node* current = node;
    while (current) {
        auto* element = dynamic_cast<const DOM::Element*>(current);
        if (element) {
            const auto& tag = element->get_tag_name();
            if (tag == Hummingbird::Html::TagNames::Button) {
                if (const auto* type = element->find_attribute(Hummingbird::Html::AttributeNames::Type)) {
                    if (!type->empty() && !Core::Utils::equals_ignore_case(*type, "submit")) {
                        current = current->get_parent();
                        continue;
                    }
                }
                return element;
            }
            if (tag == Hummingbird::Html::TagNames::Input) {
                if (const auto* type = element->find_attribute(Hummingbird::Html::AttributeNames::Type)) {
                    if (Core::Utils::equals_ignore_case(*type, "submit")) {
                        return element;
                    }
                }
            }
        }
        current = current->get_parent();
    }
    return nullptr;
}

std::string describe_submit_target(const DOM::Element* element) {
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
}  // namespace

std::optional<std::string> DocumentNavigation::hit_test_link(const Layout::RenderObject* render_tree,
                                                             const Layout::Point& point, const Layout::Rect& viewport,
                                                             float scroll_y, std::string_view base_url) const {
    return HitTest::hit_test_z_order<std::string>(
        render_tree, point, viewport, scroll_y,
        [&](const Layout::RenderObject& node) { return resolve_anchor_href(node.get_dom_node(), base_url); });
}

std::optional<FormSubmission> DocumentNavigation::submit_form_at(const Layout::RenderObject* render_tree,
                                                                 const Layout::Point& point,
                                                                 const Layout::Rect& viewport, float scroll_y,
                                                                 std::string_view base_url) const {
    return HitTest::hit_test_z_order<FormSubmission>(
        render_tree, point, viewport, scroll_y, [&](const Layout::RenderObject& node) -> std::optional<FormSubmission> {
            const auto* submit = resolve_submit_element(node.get_dom_node());
            if (!submit) {
                return std::nullopt;
            }
            HB_LOG_DEBUG("[input] submit_form_at point=(" << point.x << "," << point.y
                                                          << ") hit=" << describe_submit_target(submit));
            return model_.build_form_submission(*submit, base_url);
        });
}

}  // namespace Hummingbird::Engine
