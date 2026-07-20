#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

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
    // window.location / fragment navigation (7.2.5).
    void set_location(std::string_view url);
    DispatchResult navigate_fragment(DocumentModel& model, std::string_view url);
    // Timers (7.3.1): fire due setTimeout/setInterval callbacks at `now_ms`.
    DispatchResult run_timers(DocumentModel& model, double now_ms);
    bool has_pending_timers() const;
    // requestAnimationFrame (7.3.3): fire this frame's callbacks.
    DispatchResult run_animation_frames(DocumentModel& model, double now_ms);
    bool has_pending_animation_frames() const;
    // Script-initiated location.hash change to reflect in chrome/history (7.7.3).
    std::optional<std::string> consume_location_change();

private:
    std::unique_ptr<DocumentScriptController> controller_;
};

}  // namespace Hummingbird::Engine
