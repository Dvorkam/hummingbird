#pragma once

#include <string>
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

    static JSValue js_node_get_class_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_set_class_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_inner_html(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_set_inner_html(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // Form-control surface (7.1.5).
    static JSValue js_node_get_value(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_set_value(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_checked(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_set_checked(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_disabled(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_set_disabled(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_focus(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_blur(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_class_list(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_get_dataset(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // Node/Element methods.
    static JSValue js_node_append_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_insert_before(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_remove_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_replace_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_set_attribute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_get_attribute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_remove_attribute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // Selector queries. Shared by document.* and element.*: the scope is
    // node_from_value(this_val), which is null (== document root) when called on
    // the plain `document` object.
    static JSValue js_query_selector(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_query_selector_all(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_matches(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_element_closest(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_get_elements_by_class_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_get_elements_by_tag_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // DOMTokenList (classList) methods.
    static JSValue js_token_list_add(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_token_list_remove(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_token_list_toggle(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_token_list_contains(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // DOMStringMap (dataset) exotic property access.
    static JSValue js_string_map_get(JSContext* ctx, JSValueConst obj, JSAtom atom, JSValueConst receiver);
    static int js_string_map_set(JSContext* ctx, JSValueConst obj, JSAtom atom, JSValueConst value,
                                 JSValueConst receiver, int flags);

    // EventTarget (7.2.1).
    static JSValue js_node_add_event_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_remove_event_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_node_dispatch_event(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    // Event object methods (7.2.2). The Event is a plain object; these set flags
    // the C++ dispatch loop reads back (defaultPrevented / propagation state).
    static JSValue js_event_prevent_default(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_event_stop_propagation(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_event_stop_immediate_propagation(JSContext* ctx, JSValueConst this_val, int argc,
                                                       JSValueConst* argv);

    static JSValue js_native_insert_css(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

    void install_node_prototype();
    void install_token_list_class();
    void install_string_map_class();
    void install_console_bindings();
    void install_document_bindings();
    void install_extension_bindings();

    void define_getter(JSValueConst proto, const char* name, JSCFunction* getter);
    void define_accessor(JSValueConst proto, const char* name, JSCFunction* getter, JSCFunction* setter);
    void define_method(JSValueConst proto, const char* name, JSCFunction* method, int length);

    JSValue wrap_node(DOM::Node* node);
    JSValue wrap_node_list(const std::vector<DOM::Node*>& nodes);
    DOM::Node* node_from_value(JSValueConst value);
    // Node backing a DOMTokenList/DOMStringMap wrapper (opaque is the raw node).
    DOM::Node* node_from_opaque(JSValueConst value, JSClassID class_id);

    // Reads the capture flag from addEventListener/removeEventListener's optional
    // third argument (a boolean or an options object with a `capture` field).
    bool read_capture_flag(JSValueConst options) const;
    // Synchronously invokes every listener registered for `type` on `node`, in
    // registration order, with `this` == the node wrapper and `event` as the arg.
    // Honors stopImmediatePropagation within the node. (Target-phase only for
    // 7.2.1/7.2.2; capture/bubble propagation across ancestors arrives in 7.2.3.)
    void invoke_listeners(DOM::Node* node, const std::string& type, JSValueConst event);
    void free_listeners();

    // Builds the Event object handed to listeners: `type`/`target` plus
    // preventDefault/stopPropagation/stopImmediatePropagation and their flags.
    JSValue make_event(const std::string& type, DOM::Node* target);
    // Reads a boolean flag property off an Event object.
    bool event_flag(JSValueConst event, const char* name) const;

    JSRuntime* runtime_ = nullptr;
    JSContext* context_ = nullptr;
    JSClassID node_class_id_ = 0;
    JSClassID token_list_class_id_ = 0;
    JSClassID string_map_class_id_ = 0;
    bool console_ready_ = false;
    bool document_ready_ = false;
    bool extension_ready_ = false;

    // Identity cache: one JS wrapper per DOM node for the document's lifetime, so
    // `a.firstChild === a.firstChild` and node-keyed Sets work. Freed on
    // reset_bindings (navigation), before the node pointers dangle.
    std::unordered_map<DOM::Node*, JSValue> node_wrappers_;

    // Per-node event listeners (EventTarget registry, 7.2.1). Keyed by the raw
    // arena node; each entry owns a reference to its JS callback. Freed on
    // reset_bindings so no callback outlives the document.
    struct EventListener {
        std::string type;
        JSValue callback;
        bool capture;
    };
    std::unordered_map<DOM::Node*, std::vector<EventListener>> listeners_;
};

}  // namespace Hummingbird::Platform
