#include "platform/script/QuickJSScriptEngine.h"

#include <string>
#include <utility>

#include "core/utils/Log.h"

namespace Hummingbird::Platform {

QuickJSScriptEngine* QuickJSScriptEngine::engine_from_context(JSContext* ctx) {
    return static_cast<QuickJSScriptEngine*>(JS_GetContextOpaque(ctx));
}

static DOM::Element* element_from_value(JSContext* ctx, JSValueConst value, JSClassID class_id) {
    return static_cast<DOM::Element*>(JS_GetOpaque2(ctx, value, class_id));
}

JSValue QuickJSScriptEngine::js_console_log(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    std::string message;
    for (int i = 0; i < argc; ++i) {
        const char* text = JS_ToCString(ctx, argv[i]);
        if (text) {
            if (i > 0) {
                message.push_back(' ');
            }
            message.append(text);
            JS_FreeCString(ctx, text);
        }
    }
    HB_LOG_INFO("[js] " << message);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_document_get_element_by_id(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                           JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) {
        return JS_NULL;
    }
    if (argc < 1) {
        return JS_NULL;
    }
    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) {
        return JS_NULL;
    }
    auto* element = engine->host_->get_element_by_id(id);
    JS_FreeCString(ctx, id);
    if (!element) {
        return JS_NULL;
    }
    return engine->wrap_element(element);
}

JSValue QuickJSScriptEngine::js_element_get_text_content(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                         JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) {
        return JS_UNDEFINED;
    }
    auto* element = element_from_value(ctx, this_val, engine->element_class_id_);
    if (!element) {
        return JS_UNDEFINED;
    }
    std::string text = engine->host_->get_text_content(element);
    return JS_NewString(ctx, text.c_str());
}

JSValue QuickJSScriptEngine::js_element_set_text_content(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) {
        return JS_UNDEFINED;
    }
    auto* element = element_from_value(ctx, this_val, engine->element_class_id_);
    if (!element || argc < 1) {
        return JS_UNDEFINED;
    }
    const char* value = JS_ToCString(ctx, argv[0]);
    if (!value) {
        return JS_UNDEFINED;
    }
    engine->host_->set_text_content(element, value);
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_element_set_attribute(JSContext* ctx, JSValueConst this_val, int argc,
                                                      JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) {
        return JS_UNDEFINED;
    }
    auto* element = element_from_value(ctx, this_val, engine->element_class_id_);
    if (!element || argc < 2) {
        return JS_UNDEFINED;
    }
    const char* name = JS_ToCString(ctx, argv[0]);
    const char* value = JS_ToCString(ctx, argv[1]);
    if (name && value) {
        engine->host_->set_attribute(element, name, value);
    }
    if (name) {
        JS_FreeCString(ctx, name);
    }
    if (value) {
        JS_FreeCString(ctx, value);
    }
    return JS_UNDEFINED;
}

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
        return;
    }

    JS_SetContextOpaque(context_, this);
    JS_NewClassID(runtime_, &element_class_id_);
    JSClassDef class_def{};
    class_def.class_name = "Element";
    JS_NewClass(runtime_, element_class_id_, &class_def);

    // Install console bindings unconditionally so non-DOM scripts (e.g., extensions)
    // can log without needing to bind a host.
    install_console_bindings();
}

QuickJSScriptEngine::~QuickJSScriptEngine() {
    if (context_) {
        clear_bindings();
        JS_FreeContext(context_);
        context_ = nullptr;
    }
    if (runtime_) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

void QuickJSScriptEngine::bind_host(IScriptHost* host) {
    ScriptEngineBase::bind_host(host);
    if (!context_) {
        return;
    }
    install_console_bindings();
    if (host) {
        install_document_bindings();
    }
}

ScriptEvalResult QuickJSScriptEngine::eval(std::string_view source, std::string_view filename) {
    if (!context_) {
        return error_result("QuickJS runtime unavailable");
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
        return error_result(std::move(error));
    }
    JS_FreeValue(context_, result);
    return ok_result();
}

void QuickJSScriptEngine::install_console_bindings() {
    if (console_ready_) {
        return;
    }
    if (!context_) {
        return;
    }
    JSValue global = JS_GetGlobalObject(context_);

    JSValue console = JS_NewObject(context_);
    JS_SetPropertyStr(context_, console, "log", JS_NewCFunction(context_, js_console_log, "log", 1));
    JS_SetPropertyStr(context_, global, "console", console);

    JS_FreeValue(context_, global);
    console_ready_ = true;
}

void QuickJSScriptEngine::install_document_bindings() {
    if (document_ready_) {
        return;
    }
    if (!context_) {
        return;
    }

    JSValue global = JS_GetGlobalObject(context_);
    JSValue document = JS_NewObject(context_);
    JS_SetPropertyStr(context_, document, "getElementById",
                      JS_NewCFunction(context_, js_document_get_element_by_id, "getElementById", 1));
    JS_SetPropertyStr(context_, global, "document", document);
    JS_FreeValue(context_, global);
    document_ready_ = true;
}

void QuickJSScriptEngine::clear_bindings() {
    console_ready_ = false;
    document_ready_ = false;
}

JSValue QuickJSScriptEngine::wrap_element(DOM::Element* element) {
    if (!context_ || !element || element_class_id_ == 0) {
        return JS_NULL;
    }
    JSValue obj = JS_NewObjectClass(context_, element_class_id_);
    if (JS_IsException(obj)) {
        return obj;
    }
    JS_SetOpaque(obj, element);

    JSAtom text_atom = JS_NewAtom(context_, "textContent");
    JSValue getter = JS_NewCFunction(context_, js_element_get_text_content, "getTextContent", 0);
    JSValue setter = JS_NewCFunction(context_, js_element_set_text_content, "setTextContent", 1);
    JS_DefinePropertyGetSet(context_, obj, text_atom, getter, setter, JS_PROP_CONFIGURABLE);
    JS_FreeAtom(context_, text_atom);

    JS_SetPropertyStr(context_, obj, "setAttribute",
                      JS_NewCFunction(context_, js_element_set_attribute, "setAttribute", 2));

    return obj;
}

}  // namespace Hummingbird::Platform
