#pragma once

#include <string_view>

#include "platform/script/ScriptEngineBase.h"

extern "C" {
#include <quickjs.h>
}

namespace Hummingbird::Platform {

class QuickJSScriptEngine final : public ScriptEngineBase {
public:
    QuickJSScriptEngine();
    ~QuickJSScriptEngine() override;

    void bind_host(IScriptHost* host) override;
    ScriptEvalResult eval(std::string_view source, std::string_view filename) override;

private:
    static QuickJSScriptEngine* engine_from_context(JSContext* ctx);
    static JSValue js_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_document_get_element_by_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_get_text_content(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_set_text_content(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_set_attribute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    void install_bindings();
    void clear_bindings();
    JSValue wrap_element(DOM::Element* element);

    JSRuntime* runtime_ = nullptr;
    JSContext* context_ = nullptr;
    JSClassID element_class_id_ = 0;
    bool bindings_ready_ = false;
};

}  // namespace Hummingbird::Platform
