#include "platform/script/QuickJSScriptEngine.h"

#include <utility>

#include "core/utils/Log.h"

extern "C" {
#include <quickjs.h>
}

namespace Hummingbird::Platform {

QuickJSScriptEngine::QuickJSScriptEngine() {
    runtime_ = JS_NewRuntime();
    if (!runtime_) {
        HB_LOG_ERROR("[script] failed to create QuickJS runtime");
        return;
    }
    context_ = JS_NewContext(runtime_);
    if (!context_) {
        HB_LOG_ERROR("[script] failed to create QuickJS context");
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

QuickJSScriptEngine::~QuickJSScriptEngine() {
    if (context_) {
        JS_FreeContext(context_);
        context_ = nullptr;
    }
    if (runtime_) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

ScriptEvalResult QuickJSScriptEngine::eval(std::string_view source, std::string_view filename) {
    if (!context_) {
        return {false, "QuickJS runtime unavailable"};
    }
    std::string filename_str(filename);
    JSValue result = JS_Eval(context_, source.data(), source.size(), filename_str.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(context_);
        const char* message = JS_ToCString(context_, exception);
        std::string error = message ? message : "Unknown JS exception";
        if (message) {
            JS_FreeCString(context_, message);
        }
        JS_FreeValue(context_, exception);
        JS_FreeValue(context_, result);
        return {false, std::move(error)};
    }
    JS_FreeValue(context_, result);
    return {true, {}};
}

}  // namespace Hummingbird::Platform
