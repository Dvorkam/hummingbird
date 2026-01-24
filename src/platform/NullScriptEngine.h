#pragma once

#include "core/platform_api/IScriptEngine.h"

namespace Hummingbird::Platform {

class NullScriptEngine final : public IScriptEngine {
public:
    ScriptEvalResult eval(std::string_view source, std::string_view filename) override;
};

}  // namespace Hummingbird::Platform
