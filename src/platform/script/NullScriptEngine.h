#pragma once

#include "platform/script/ScriptEngineBase.h"

namespace Hummingbird::Platform {

class NullScriptEngine final : public ScriptEngineBase {
public:
    ScriptEvalResult eval(std::string_view source, std::string_view filename) override;
};

}  // namespace Hummingbird::Platform
