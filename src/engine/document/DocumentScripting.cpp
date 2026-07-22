#include "engine/document/DocumentScripting.h"

#include <utility>
#include <vector>

#include "core/utils/Log.h"
#include "engine/document/DocumentModel.h"
#include "engine/script/DocumentScriptController.h"

namespace Hummingbird::Engine {

DocumentScripting::DocumentScripting(ScriptEnginePtr script_engine)
    : controller_(std::make_unique<DocumentScriptController>(std::move(script_engine))) {}

DocumentScripting::~DocumentScripting() = default;

void DocumentScripting::reset() {
    controller_->clear();
}

void DocumentScripting::set_focus_sink(std::function<void(DOM::Element*, bool)> sink) {
    controller_->set_focus_sink(std::move(sink));
}

void DocumentScripting::set_cookie_accessors(std::function<std::string()> reader,
                                             std::function<void(std::string_view)> writer) {
    controller_->set_cookie_accessors(std::move(reader), std::move(writer));
}

void DocumentScripting::set_storage_accessor(std::function<Core::StorageArea*()> accessor) {
    controller_->set_storage_accessor(std::move(accessor));
}

bool DocumentScripting::run_document_scripts(DocumentModel& model, const ExternalScriptLookup& external_lookup) {
    std::vector<DocumentScriptController::ScriptSource> sources;
    const auto& scripts = model.document_scripts();
    sources.reserve(scripts.size());
    for (const auto& script : scripts) {
        if (!script.is_external()) {
            sources.push_back({script.text, "inline"});
            continue;
        }
        std::optional<std::string_view> body;
        if (external_lookup) {
            body = external_lookup(script.src);
        }
        if (!body) {
            HB_LOG_WARN("[script] external script skipped (not loaded): " << script.src);
            continue;
        }
        sources.push_back({*body, script.src});
    }
    return controller_->run_scripts(sources, model.dom_root(), model.dom_arena());
}

DocumentScripting::DispatchResult DocumentScripting::dispatch_click(DocumentModel& model, const Layout::Rect& viewport,
                                                                    const Layout::Point& point, float scroll_y,
                                                                    int click_count) {
    auto result = controller_->dispatch_click(model.dom_root(), model.dom_arena(), model.render_tree(), viewport, point,
                                              scroll_y, click_count);
    return {result.handled, result.mutated, result.default_prevented};
}

DocumentScripting::DispatchResult DocumentScripting::dispatch_load(DocumentModel& model) {
    auto result = controller_->dispatch_load(model.dom_root(), model.dom_arena());
    return {result.handled, result.mutated};
}

DocumentScripting::DispatchResult DocumentScripting::dispatch_dom_event(DocumentModel& model, DOM::Node* target,
                                                                        const ScriptDomEvent& event) {
    auto result = controller_->dispatch_dom_event(model.dom_root(), model.dom_arena(), target, event);
    return {result.handled, result.mutated, result.default_prevented};
}

void DocumentScripting::set_location(std::string_view url) {
    controller_->set_location(url);
}

DocumentScripting::DispatchResult DocumentScripting::navigate_fragment(DocumentModel& model, std::string_view url) {
    auto result = controller_->navigate_fragment(model.dom_root(), model.dom_arena(), url);
    return {result.handled, result.mutated, result.default_prevented};
}

DocumentScripting::DispatchResult DocumentScripting::run_timers(DocumentModel& model, double now_ms) {
    auto result = controller_->run_timers(model.dom_root(), model.dom_arena(), now_ms);
    return {result.handled, result.mutated, result.default_prevented};
}

bool DocumentScripting::has_pending_timers() const {
    return controller_->has_pending_timers();
}

DocumentScripting::DispatchResult DocumentScripting::run_animation_frames(DocumentModel& model, double now_ms) {
    auto result = controller_->run_animation_frames(model.dom_root(), model.dom_arena(), now_ms);
    return {result.handled, result.mutated, result.default_prevented};
}

bool DocumentScripting::has_pending_animation_frames() const {
    return controller_->has_pending_animation_frames();
}

std::optional<std::string> DocumentScripting::consume_location_change() {
    return controller_->consume_location_change();
}

}  // namespace Hummingbird::Engine
