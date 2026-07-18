#pragma once

#include <functional>
#include <memory>

#include "core/platform_api/IScriptEngine.h"
#include "engine/document/ExternalScriptLookup.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::DOM {
class Element;
}

namespace Hummingbird::Engine {

class DocumentModel;
class DocumentScriptController;

class DocumentScripting {
public:
    struct DispatchResult {
        bool handled = false;
        bool mutated = false;
        bool default_prevented = false;
    };

    explicit DocumentScripting(ScriptEnginePtr script_engine);
    ~DocumentScripting();

    DocumentScripting(const DocumentScripting&) = delete;
    DocumentScripting& operator=(const DocumentScripting&) = delete;
    DocumentScripting(DocumentScripting&&) = delete;
    DocumentScripting& operator=(DocumentScripting&&) = delete;

    void reset();
    // Routes JS focus()/blur() to the caret target (wired once by DocumentPipeline).
    void set_focus_sink(std::function<void(DOM::Element*, bool)> sink);
    // Runs the document's <script>s in document order, inline and external
    // interleaved (classic-script semantics, 7.0.1 MVP).
    bool run_document_scripts(DocumentModel& model, const ExternalScriptLookup& external_lookup);
    DispatchResult dispatch_click(DocumentModel& model, const Layout::Rect& viewport, const Layout::Point& point,
                                  float scroll_y, int click_count = 1);
    DispatchResult dispatch_load(DocumentModel& model);
    // Dispatches a DOM event (keyboard/input/etc.) to `target` node.
    DispatchResult dispatch_dom_event(DocumentModel& model, DOM::Node* target, const ScriptDomEvent& event);

private:
    std::unique_ptr<DocumentScriptController> controller_;
};

}  // namespace Hummingbird::Engine
