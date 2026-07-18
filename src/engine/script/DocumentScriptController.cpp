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
    // Drop cached node wrappers before the host forgets the document: their
    // opaque pointers become dangling once the DOM arena is reset on navigation.
    if (script_engine_) {
        script_engine_->reset_bindings();
    }
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
    const Layout::Rect& viewport, const Layout::Point& point, float scroll_y, int click_count) {
    if (!bind_host(dom_root, arena)) {
        return {};
    }

    // The event target is the topmost DOM node under the cursor.
    auto target = HitTest::hit_test_z_order<DOM::Node*>(
        render_tree, point, viewport, scroll_y, [](const Layout::RenderObject& node) -> std::optional<DOM::Node*> {
            if (const auto* dom = node.get_dom_node()) {
                return const_cast<DOM::Node*>(dom);
            }
            return std::nullopt;
        });

    bool default_prevented = false;
    if (target && script_engine_) {
        // A real DOM click (bubbling, cancelable): addEventListener('click') runs
        // through the full capture/target/bubble pipeline; preventDefault cancels
        // the default action (link navigation, handled by the caller).
        if (!script_engine_->dispatch_dom_event(*target, ScriptDomEvent{"click", true, true, "", ""})) {
            default_prevented = true;
        }
        if (click_count >= 2) {
            script_engine_->dispatch_dom_event(*target, ScriptDomEvent{"dblclick", true, true, "", ""});
        }
    }

    // Legacy inline onclick="" attribute handler (walks ancestors for the nearest).
    bool handled = false;
    auto handler = HitTest::hit_test_z_order<std::string>(
        render_tree, point, viewport, scroll_y,
        [&](const Layout::RenderObject& node) { return resolve_onclick_handler(node.get_dom_node()); });
    if (handler && script_engine_) {
        std::string wrapped = "(function(){";
        wrapped.append(*handler);
        wrapped.append("})();");
        auto result = script_engine_->eval(wrapped, "onclick");
        if (!result.ok) {
            HB_LOG_WARN("[script] onclick eval failed: " << result.error);
        }
        handled = true;
    }

    return {handled, script_host_.consume_mutations(), default_prevented};
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
