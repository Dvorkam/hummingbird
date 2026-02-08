#pragma once

#include <memory>
#include <string>
#include <string_view>

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
    virtual ScriptEvalResult eval(std::string_view source, std::string_view filename = "inline") = 0;
};

using ScriptEnginePtr = std::unique_ptr<IScriptEngine>;

}  // namespace Hummingbird
