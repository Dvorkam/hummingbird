#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "core/platform_api/IScriptEngine.h"
#include "engine/script/DocumentScriptHost.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::Core {
class ArenaAllocator;
}

namespace Hummingbird::DOM {
class Node;
class Element;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentScriptController final {
public:
    struct ScriptDispatchResult {
        bool handled = false;
        bool mutated = false;
        bool default_prevented = false;  // a listener called preventDefault
    };

    // One script body ready to eval; views must stay valid for the run_scripts
    // call (they point into the document model / resource store).
    struct ScriptSource {
        std::string_view text;
        std::string_view context_name;
    };

    explicit DocumentScriptController(ScriptEnginePtr engine);

    void clear();
    // Routes JS focus()/blur() to the caret target (wired by the pipeline).
    void set_focus_sink(std::function<void(DOM::Element*, bool)> sink);

    bool run_scripts(const std::vector<ScriptSource>& scripts, DOM::Node* dom_root, Core::ArenaAllocator* arena);
    ScriptDispatchResult dispatch_click(DOM::Node* dom_root, Core::ArenaAllocator* arena,
                                        const Layout::RenderObject* render_tree, const Layout::Rect& viewport,
                                        const Layout::Point& point, float scroll_y, int click_count = 1);
    ScriptDispatchResult dispatch_load(DOM::Node* dom_root, Core::ArenaAllocator* arena);
    // Dispatches an already-built DOM event to `target` (keyboard/input/etc.);
    // the caller decides the target node and event fields.
    ScriptDispatchResult dispatch_dom_event(DOM::Node* dom_root, Core::ArenaAllocator* arena, DOM::Node* target,
                                            const ScriptDomEvent& event);

private:
    bool bind_host(DOM::Node* dom_root, Core::ArenaAllocator* arena);
    ScriptDispatchResult eval_inline_script(DOM::Node* dom_root, Core::ArenaAllocator* arena, std::string_view script,
                                            std::string_view context_name);

    ScriptEnginePtr script_engine_;
    DocumentScriptHost script_host_;
};

}  // namespace Hummingbird::Engine
