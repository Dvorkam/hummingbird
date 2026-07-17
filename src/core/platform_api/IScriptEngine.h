#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "core/platform_api/IExtensionApiHost.h"
#include "core/platform_api/IScriptHost.h"

namespace Hummingbird {

struct ScriptEvalResult {
    bool ok = false;
    std::string error;
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
};

using ScriptEnginePtr = std::unique_ptr<IScriptEngine>;

}  // namespace Hummingbird
