#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace Hummingbird {

struct ScriptEvalResult {
    bool ok = false;
    std::string error;
};

class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    virtual ScriptEvalResult eval(std::string_view source, std::string_view filename = "inline") = 0;
};

using ScriptEnginePtr = std::unique_ptr<IScriptEngine>;

}  // namespace Hummingbird
