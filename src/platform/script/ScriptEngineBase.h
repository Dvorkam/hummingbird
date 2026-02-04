#pragma once

#include <string>
#include <utility>

#include "core/platform_api/IScriptEngine.h"

namespace Hummingbird::Platform {

class ScriptEngineBase : public IScriptEngine {
public:
    void bind_host(IScriptHost* host) override { host_ = host; }

protected:
    static ScriptEvalResult ok_result() { return {true, {}}; }
    static ScriptEvalResult error_result(std::string message) { return {false, std::move(message)}; }

    IScriptHost* host_ = nullptr;
};

}  // namespace Hummingbird::Platform
