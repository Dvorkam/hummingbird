#pragma once

#include <string_view>

#include "core/platform_api/IScriptEngine.h"
#include "engine/script/DocumentScriptHost.h"
#include "layout/Geometry.h"

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentModel;

class DocumentScriptController final {
public:
    struct ScriptDispatchResult {
        bool handled = false;
        bool mutated = false;
    };

    explicit DocumentScriptController(ScriptEnginePtr engine);

    void clear();

    bool run_inline_scripts(DocumentModel& model);
    ScriptDispatchResult dispatch_click(DocumentModel& model, const Layout::RenderObject* render_tree,
                                        const Layout::Rect& viewport, const Layout::Point& point, float scroll_y);
    ScriptDispatchResult dispatch_load(DocumentModel& model);

private:
    bool bind_host(DocumentModel& model);
    ScriptDispatchResult eval_inline_script(DocumentModel& model, std::string_view script,
                                            std::string_view context_name);

    ScriptEnginePtr script_engine_;
    DocumentScriptHost script_host_;
};

}  // namespace Hummingbird::Engine
