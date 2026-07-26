#include "engine/script/DocumentScriptController.h"

#include <optional>
#include <utility>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "engine/document/DocumentInputUtils.h"
#include "engine/document/HitTestUtils.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/PositioningUtils.h"

namespace Hummingbird::Engine {

namespace {
// RAII: brackets one script dispatch. Nested dispatches (a host callback that
// re-enters the controller) share the outer dispatch's mutation epoch, so the
// inner bind_host's reset does not wipe the outer's accumulated mutated flag
// and only the outermost dispatch consumes it (T-DISPATCH-REENTRANT-1, 7.7.1).
class DispatchScope {
public:
    explicit DispatchScope(DocumentScriptHost& host) : host_(host) { host_.begin_dispatch(); }
    ~DispatchScope() { host_.end_dispatch(); }
    DispatchScope(const DispatchScope&) = delete;
    DispatchScope& operator=(const DispatchScope&) = delete;

private:
    DocumentScriptHost& host_;
};

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

void DocumentScriptController::set_focus_sink(std::function<void(DOM::Element*, bool)> sink) {
    script_host_.set_focus_sink(std::move(sink));
}

void DocumentScriptController::set_cookie_accessors(std::function<std::string()> reader,
                                                    std::function<void(std::string_view)> writer) {
    script_host_.set_cookie_accessors(std::move(reader), std::move(writer));
}

void DocumentScriptController::set_storage_accessor(std::function<Core::StorageArea*()> accessor) {
    script_host_.set_storage_accessor(std::move(accessor));
}

void DocumentScriptController::set_session_storage_accessor(std::function<Core::StorageArea*()> accessor) {
    script_host_.set_session_storage_accessor(std::move(accessor));
}

void DocumentScriptController::set_location(std::string_view url) {
    if (script_engine_) {
        script_engine_->set_location(url);
    }
}

DocumentScriptController::ScriptDispatchResult DocumentScriptController::navigate_fragment(DOM::Node* dom_root,
                                                                                           Core::ArenaAllocator* arena,
                                                                                           std::string_view url) {
    DispatchScope scope(script_host_);
    // Bind the host so a hashchange listener's DOM mutations are captured.
    if (!bind_host(dom_root, arena) || !script_engine_) {
        return {};
    }
    const bool hash_changed = script_engine_->navigate_fragment(url);
    return {hash_changed, script_host_.consume_mutations(), false};
}

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
    DispatchScope scope(script_host_);
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
    DispatchScope scope(script_host_);
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
        // Checkbox pre-click activation: flip checkedness before the click event
        // fires, so listeners observe the post-toggle state (matches real UAs).
        // If the click is cancelled, the toggle is reverted below.
        bool is_checkbox = is_checkbox_input_element(dynamic_cast<DOM::Element*>(*target));
        bool checked_before_click = is_checkbox && script_host_.get_checked(*target);
        if (is_checkbox) {
            script_host_.set_checked(*target, !checked_before_click);
        }

        // A real DOM click (bubbling, cancelable): addEventListener('click') runs
        // through the full capture/target/bubble pipeline; preventDefault cancels
        // the default action (link navigation, handled by the caller).
        if (!script_engine_->dispatch_dom_event(*target, ScriptDomEvent{"click", true, true, "", ""})) {
            default_prevented = true;
        }
        if (click_count >= 2) {
            script_engine_->dispatch_dom_event(*target, ScriptDomEvent{"dblclick", true, true, "", ""});
        }

        if (is_checkbox) {
            if (default_prevented) {
                script_host_.set_checked(*target, checked_before_click);
            } else {
                script_engine_->dispatch_dom_event(*target, ScriptDomEvent{"input", true, false, "", ""});
                script_engine_->dispatch_dom_event(*target, ScriptDomEvent{"change", true, false, "", ""});
            }
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

DocumentScriptController::ScriptDispatchResult DocumentScriptController::dispatch_dom_event(
    DOM::Node* dom_root, Core::ArenaAllocator* arena, DOM::Node* target, const ScriptDomEvent& event) {
    DispatchScope scope(script_host_);
    if (!target || !bind_host(dom_root, arena) || !script_engine_) {
        return {};
    }
    const bool not_prevented = script_engine_->dispatch_dom_event(target, event);
    return {true, script_host_.consume_mutations(), !not_prevented};
}

DocumentScriptController::ScriptDispatchResult DocumentScriptController::run_timers(DOM::Node* dom_root,
                                                                                    Core::ArenaAllocator* arena,
                                                                                    double now_ms) {
    if (!script_engine_ || !script_engine_->has_pending_timers()) {
        return {};
    }
    DispatchScope scope(script_host_);
    if (!bind_host(dom_root, arena)) {
        return {};
    }
    const bool fired = script_engine_->run_due_timers(now_ms);
    return {fired, script_host_.consume_mutations()};
}

bool DocumentScriptController::has_pending_timers() const {
    return script_engine_ && script_engine_->has_pending_timers();
}

DocumentScriptController::ScriptDispatchResult DocumentScriptController::run_animation_frames(
    DOM::Node* dom_root, Core::ArenaAllocator* arena, double now_ms) {
    if (!script_engine_ || !script_engine_->has_pending_animation_frames()) {
        return {};
    }
    DispatchScope scope(script_host_);
    if (!bind_host(dom_root, arena)) {
        return {};
    }
    const bool fired = script_engine_->run_animation_frames(now_ms);
    return {fired, script_host_.consume_mutations()};
}

bool DocumentScriptController::has_pending_animation_frames() const {
    return script_engine_ && script_engine_->has_pending_animation_frames();
}

std::optional<std::string> DocumentScriptController::consume_location_change() {
    return script_engine_ ? script_engine_->consume_location_change() : std::nullopt;
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
    DispatchScope scope(script_host_);
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
