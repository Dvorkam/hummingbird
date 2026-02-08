#pragma once

#include <vector>
#include <string_view>

#include "core/platform_api/IScriptEngine.h"
#include "engine/script/DocumentScriptHost.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::Core {
class ArenaAllocator;
}

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentScriptController final {
public:
    struct ScriptDispatchResult {
        bool handled = false;
        bool mutated = false;
    };

    explicit DocumentScriptController(ScriptEnginePtr engine);

    void clear();

    bool run_inline_scripts(const std::vector<std::string>& scripts, DOM::Node* dom_root, Core::ArenaAllocator* arena);
    ScriptDispatchResult dispatch_click(DOM::Node* dom_root, Core::ArenaAllocator* arena,
                                        const Layout::RenderObject* render_tree,
                                        const Layout::Rect& viewport, const Layout::Point& point, float scroll_y);
    ScriptDispatchResult dispatch_load(DOM::Node* dom_root, Core::ArenaAllocator* arena);

private:
    bool bind_host(DOM::Node* dom_root, Core::ArenaAllocator* arena);
    ScriptDispatchResult eval_inline_script(DOM::Node* dom_root, Core::ArenaAllocator* arena, std::string_view script,
                                            std::string_view context_name);

    ScriptEnginePtr script_engine_;
    DocumentScriptHost script_host_;
};

}  // namespace Hummingbird::Engine
