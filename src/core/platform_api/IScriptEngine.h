#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "core/platform_api/IExtensionApiHost.h"
#include "core/platform_api/IScriptHost.h"

namespace Hummingbird {

namespace DOM {
class Node;
}

struct ScriptEvalResult {
    bool ok = false;
    std::string error;
};

// A DOM event the engine (app/input side) asks the script engine to dispatch to a
// target node through the 7.2.3 propagation pipeline (7.2.4). Platform-agnostic:
// the engine fills the fields, the adapter builds the JS Event.
struct ScriptDomEvent {
    std::string type;
    bool bubbles = true;
    bool cancelable = true;
    std::string key;   // keyboard events
    std::string code;  // keyboard events
};

class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    virtual void bind_host(IScriptHost* host) = 0;
    virtual void bind_extension_host(IExtensionApiHost* host) = 0;
    virtual ScriptEvalResult eval(std::string_view source, std::string_view filename = "inline") = 0;

    // Drops any per-document state the engine caches across evals (e.g. DOM node
    // wrappers holding raw node pointers). Called on navigation, before the
    // document's arena is reset, so no stale handle survives into the next page.
    virtual void reset_bindings() {}

    // Dispatches `event` to `target` through the DOM event pipeline (capture/
    // target/bubble). Returns false when a listener called preventDefault (the
    // caller should skip the default action). Default: no-op returning true.
    virtual bool dispatch_dom_event(DOM::Node* /*target*/, const ScriptDomEvent& /*event*/) { return true; }
};

using ScriptEnginePtr = std::unique_ptr<IScriptEngine>;

}  // namespace Hummingbird
