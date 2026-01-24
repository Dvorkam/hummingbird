#pragma once

#include <string_view>

#include "core/platform_api/IScriptEngine.h"

struct JSRuntime;
struct JSContext;

namespace Hummingbird::Platform {

class QuickJSScriptEngine final : public IScriptEngine {
public:
    QuickJSScriptEngine();
    ~QuickJSScriptEngine() override;

    ScriptEvalResult eval(std::string_view source, std::string_view filename) override;

private:
    JSRuntime* runtime_ = nullptr;
    JSContext* context_ = nullptr;
};

}  // namespace Hummingbird::Platform
