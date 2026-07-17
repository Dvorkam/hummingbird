#include "engine/script/DocumentScriptController.h"

#include <optional>
#include <utility>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "engine/document/HitTestUtils.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/PositioningUtils.h"

namespace Hummingbird::Engine {

namespace {
std::optional<std::string> resolve_onclick_handler(const DOM::Node* node) {
    const DOM::Node* current = node;
    while (current) {
        auto* element = dynamic_cast<const DOM::Element*>(current);
        if (element) {
            const auto* handler = element->find_attribute("onclick");
            if (handler && !handler->empty()) {
                return *handler;
            }
        }
        current = current->get_parent();
    }
    return std::nullopt;
}

const DOM::Element* find_body_element(const DOM::Node* node) {
    if (!node) return nullptr;
    if (auto* element = dynamic_cast<const DOM::Element*>(node)) {
        if (element->get_tag_name() == Hummingbird::Html::TagNames::Body) {
            return element;
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* match = find_body_element(child.get())) {
            return match;
        }
    }
    return nullptr;
}

std::optional<std::string> resolve_onload_handler(const DOM::Node* node) {
    const DOM::Element* body = find_body_element(node);
    if (!body) {
        return std::nullopt;
    }
    const auto* handler = body->find_attribute("onload");
    if (!handler || handler->empty()) {
        return std::nullopt;
    }
    return *handler;
}
}  // namespace

DocumentScriptController::DocumentScriptController(ScriptEnginePtr engine) : script_engine_(std::move(engine)) {}

void DocumentScriptController::clear() {
    script_host_.clear();
}

bool DocumentScriptController::run_scripts(const std::vector<ScriptSource>& scripts, DOM::Node* dom_root,
                                           Core::ArenaAllocator* arena) {
    if (!script_engine_) {
        return false;
    }
    if (scripts.empty()) {
        return false;
    }
    if (!bind_host(dom_root, arena)) {
        return false;
    }

    for (const auto& script : scripts) {
        if (script.text.empty()) {
            continue;
        }
        auto result = script_engine_->eval(script.text, script.context_name);
        if (!result.ok) {
            HB_LOG_WARN("[script] eval failed (" << script.context_name << "): " << result.error);
        }
    }

    return script_host_.consume_mutations();
}

DocumentScriptController::ScriptDispatchResult DocumentScriptController::dispatch_click(
    DOM::Node* dom_root, Core::ArenaAllocator* arena, const Layout::RenderObject* render_tree,
    const Layout::Rect& viewport, const Layout::Point& point, float scroll_y) {
    auto handler = HitTest::hit_test_z_order<std::string>(
        render_tree, point, viewport, scroll_y,
        [&](const Layout::RenderObject& node) { return resolve_onclick_handler(node.get_dom_node()); });

    if (!handler) {
        return {};
    }

    std::string wrapped = "(function(){";
    wrapped.append(*handler);
    wrapped.append("})();");
    return eval_inline_script(dom_root, arena, wrapped, "onclick");
}

DocumentScriptController::ScriptDispatchResult DocumentScriptController::dispatch_load(DOM::Node* dom_root,
                                                                                       Core::ArenaAllocator* arena) {
    auto handler = resolve_onload_handler(dom_root);
    if (!handler) {
        return {};
    }

    std::string wrapped = "(function(){";
    wrapped.append(*handler);
    wrapped.append("})();");
    return eval_inline_script(dom_root, arena, wrapped, "onload");
}

bool DocumentScriptController::bind_host(DOM::Node* dom_root, Core::ArenaAllocator* arena) {
    if (!script_engine_) {
        return false;
    }
    if (!dom_root) {
        return false;
    }
    script_host_.reset(dom_root, arena);
    script_engine_->bind_host(&script_host_);
    return true;
}

DocumentScriptController::ScriptDispatchResult DocumentScriptController::eval_inline_script(
    DOM::Node* dom_root, Core::ArenaAllocator* arena, std::string_view script, std::string_view context_name) {
    if (script.empty()) {
        return {};
    }
    if (!bind_host(dom_root, arena)) {
        return {};
    }

    auto result = script_engine_->eval(script, context_name);
    if (!result.ok) {
        HB_LOG_WARN("[script] eval failed: " << result.error);
    }
    return {true, script_host_.consume_mutations()};
}

}  // namespace Hummingbird::Engine
