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

void DocumentScripting::set_session_storage_accessor(std::function<Core::StorageArea*()> accessor) {
    controller_->set_session_storage_accessor(std::move(accessor));
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
        ExternalScriptSource source;
        if (external_lookup) {
            source = external_lookup(script.src);
        }
        if (!source.body) {
            if (source.blocked_by_filter) {
                // Not a warning: a filter rule refused this on purpose, and the
                // `[filter] blocked` line already named the rule that did it.
                HB_LOG_INFO("[script] external script blocked by filter: " << script.src);
            } else {
                HB_LOG_WARN("[script] external script skipped (not loaded): " << script.src);
            }
            continue;
        }
        sources.push_back({*source.body, script.src});
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

std::optional<IScriptEngine::HistoryChange> DocumentScripting::consume_history_change() {
    return controller_->consume_history_change();
}

std::optional<int> DocumentScripting::consume_history_delta() {
    return controller_->consume_history_delta();
}

void DocumentScripting::set_history_length(size_t length) {
    controller_->set_history_length(length);
}

DocumentScripting::DispatchResult DocumentScripting::apply_popstate(DocumentModel& model, std::string_view url,
                                                                    std::string_view state) {
    auto result = controller_->apply_popstate(model.dom_root(), model.dom_arena(), url, state);
    return {result.handled, result.mutated, result.default_prevented};
}

std::vector<std::string> DocumentScripting::missing_apis() const {
    return controller_->missing_apis();
}

void DocumentScripting::set_fetch_sink(std::function<std::uint64_t(const ScriptFetchRequest&)> sink) {
    controller_->set_fetch_sink(std::move(sink));
}

void DocumentScripting::set_url_resolver(std::function<std::string(std::string_view)> resolver) {
    controller_->set_url_resolver(std::move(resolver));
}

bool DocumentScripting::settle_fetch(DocumentModel& model, const ScriptFetchResponse& response) {
    return controller_->settle_fetch(model.dom_root(), model.dom_arena(), response);
}

}  // namespace Hummingbird::Engine
