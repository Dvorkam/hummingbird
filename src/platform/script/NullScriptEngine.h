#pragma once

#include "core/platform_api/IScriptEngine.h"

namespace Hummingbird::Platform {

class NullScriptEngine final : public IScriptEngine {
public:
    void bind_host(IScriptHost* host) override;
    ScriptEvalResult eval(std::string_view source, std::string_view filename) override;

private:
    IScriptHost* host_ = nullptr;
};

}  // namespace Hummingbird::Platform
