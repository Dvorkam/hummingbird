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

}  // namespace Hummingbird::Engine
