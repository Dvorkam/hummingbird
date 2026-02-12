#include "engine/document/DocumentScripting.h"

#include <utility>

#include "engine/document/DocumentModel.h"
#include "engine/script/DocumentScriptController.h"

namespace Hummingbird::Engine {

DocumentScripting::DocumentScripting(ScriptEnginePtr script_engine)
    : controller_(std::make_unique<DocumentScriptController>(std::move(script_engine))) {}

DocumentScripting::~DocumentScripting() = default;

void DocumentScripting::reset() {
    controller_->clear();
}

bool DocumentScripting::run_inline_scripts(DocumentModel& model) {
    return controller_->run_inline_scripts(model.script_blocks(), model.dom_root(), model.dom_arena());
}

DocumentScripting::DispatchResult DocumentScripting::dispatch_click(DocumentModel& model,
                                                                    const Layout::Rect& viewport,
                                                                    const Layout::Point& point, float scroll_y) {
    auto result = controller_->dispatch_click(model.dom_root(), model.dom_arena(), model.render_tree(), viewport, point,
                                              scroll_y);
    return {result.handled, result.mutated};
}

DocumentScripting::DispatchResult DocumentScripting::dispatch_load(DocumentModel& model) {
    auto result = controller_->dispatch_load(model.dom_root(), model.dom_arena());
    return {result.handled, result.mutated};
}

}  // namespace Hummingbird::Engine
