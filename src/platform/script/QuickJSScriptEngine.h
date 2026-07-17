#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

#include "platform/script/ScriptEngineBase.h"

extern "C" {
#include <quickjs.h>
}

namespace Hummingbird::DOM {
class Node;
class Element;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Platform {

class QuickJSScriptEngine final : public ScriptEngineBase {
public:
    QuickJSScriptEngine();
    ~QuickJSScriptEngine() override;

    void bind_host(IScriptHost* host) override;
    void bind_extension_host(IExtensionApiHost* host) override;
    ScriptEvalResult eval(std::string_view source, std::string_view filename) override;
    void reset_bindings() override;

private:
    static QuickJSScriptEngine* engine_from_context(JSContext* ctx);

    static JSValue js_console_log(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // document.*
    static JSValue js_document_get_element_by_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_document_create_element(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_document_create_text_node(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // Node/Element property getters + setters (installed on the shared prototype).
    static JSValue js_node_get_node_type(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_node_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_tag_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_text_content(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_set_text_content(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_parent_node(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_first_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_last_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_next_sibling(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_previous_sibling(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_next_element_sibling(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv);
    static JSValue js_node_get_previous_element_sibling(JSContext* ctx, JSValueConst this_val, int argc,
                                                        JSValueConst* argv);
    static JSValue js_node_get_child_nodes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_children(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // Node/Element methods.
    static JSValue js_node_append_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_insert_before(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_remove_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_replace_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_set_attribute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    static JSValue js_native_insert_css(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    void install_node_prototype();
    void install_console_bindings();
    void install_document_bindings();
    void install_extension_bindings();

    void define_getter(JSValueConst proto, const char* name, JSCFunction* getter);
    void define_accessor(JSValueConst proto, const char* name, JSCFunction* getter, JSCFunction* setter);
    void define_method(JSValueConst proto, const char* name, JSCFunction* method, int length);

    JSValue wrap_node(DOM::Node* node);
    JSValue wrap_node_list(const std::vector<DOM::Node*>& nodes);
    DOM::Node* node_from_value(JSValueConst value);

    JSRuntime* runtime_ = nullptr;
    JSContext* context_ = nullptr;
    JSClassID node_class_id_ = 0;
    bool console_ready_ = false;
    bool document_ready_ = false;
    bool extension_ready_ = false;

    // Identity cache: one JS wrapper per DOM node for the document's lifetime, so
    // `a.firstChild === a.firstChild` and node-keyed Sets work. Freed on
    // reset_bindings (navigation), before the node pointers dangle.
    std::unordered_map<DOM::Node*, JSValue> node_wrappers_;
};

}  // namespace Hummingbird::Platform
