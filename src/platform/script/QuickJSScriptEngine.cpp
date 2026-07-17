#include "platform/script/QuickJSScriptEngine.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"

// The QuickJS engine is a thin adapter: it holds DOM nodes only as opaque
// handles and performs every read/mutation through IScriptHost. The core/dom
// includes above exist solely so the compiler knows Element derives from Node
// (needed to wrap an Element* as a Node*); no DOM methods are called here.

namespace Hummingbird::Platform {

QuickJSScriptEngine* QuickJSScriptEngine::engine_from_context(JSContext* ctx) {
    return static_cast<QuickJSScriptEngine*>(JS_GetContextOpaque(ctx));
}

DOM::Node* QuickJSScriptEngine::node_from_value(JSValueConst value) {
    return static_cast<DOM::Node*>(JS_GetOpaque(value, node_class_id_));
}

DOM::Node* QuickJSScriptEngine::node_from_opaque(JSValueConst value, JSClassID class_id) {
    return static_cast<DOM::Node*>(JS_GetOpaque(value, class_id));
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

// --- document.* ------------------------------------------------------------

JSValue QuickJSScriptEngine::js_document_get_element_by_id(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                           JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) {
        return JS_NULL;
    }
    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) {
        return JS_NULL;
    }
    auto* element = engine->host_->get_element_by_id(id);
    JS_FreeCString(ctx, id);
    return engine->wrap_node(element);
}

JSValue QuickJSScriptEngine::js_document_create_element(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                        JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) {
        return JS_NULL;
    }
    const char* tag = JS_ToCString(ctx, argv[0]);
    if (!tag) {
        return JS_NULL;
    }
    auto* element = engine->host_->create_element(tag);
    JS_FreeCString(ctx, tag);
    return engine->wrap_node(element);
}

JSValue QuickJSScriptEngine::js_document_create_text_node(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                          JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) {
        return JS_NULL;
    }
    const char* data = argc >= 1 ? JS_ToCString(ctx, argv[0]) : nullptr;
    auto* text = engine->host_->create_text_node(data ? data : "");
    if (data) {
        JS_FreeCString(ctx, data);
    }
    return engine->wrap_node(text);
}

// --- Node property getters/setters ----------------------------------------

JSValue QuickJSScriptEngine::js_node_get_node_type(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                   JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NewInt32(ctx, 0);
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_NewInt32(ctx, 0);
    switch (engine->host_->node_kind(node)) {
        case NodeKind::Element:
            return JS_NewInt32(ctx, 1);
        case NodeKind::Text:
            return JS_NewInt32(ctx, 3);
        case NodeKind::Other:
            break;
    }
    return JS_NewInt32(ctx, 0);
}

JSValue QuickJSScriptEngine::js_node_get_node_name(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                   JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_UNDEFINED;
    return JS_NewString(ctx, engine->host_->node_name(node).c_str());
}

JSValue QuickJSScriptEngine::js_node_get_tag_name(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                  JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (!node || engine->host_->node_kind(node) != NodeKind::Element) return JS_UNDEFINED;
    return JS_NewString(ctx, engine->host_->node_name(node).c_str());
}

JSValue QuickJSScriptEngine::js_node_get_text_content(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                      JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_UNDEFINED;
    return JS_NewString(ctx, engine->host_->get_text_content(node).c_str());
}

JSValue QuickJSScriptEngine::js_node_set_text_content(JSContext* ctx, JSValueConst this_val, int argc,
                                                      JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_UNDEFINED;
    const char* value = JS_ToCString(ctx, argv[0]);
    if (!value) return JS_UNDEFINED;
    engine->host_->set_text_content(node, value);
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_get_parent_node(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                     JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    return engine->wrap_node(node ? engine->host_->parent_node(node) : nullptr);
}

JSValue QuickJSScriptEngine::js_node_get_first_child(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                     JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    return engine->wrap_node(node ? engine->host_->first_child(node) : nullptr);
}

JSValue QuickJSScriptEngine::js_node_get_last_child(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    return engine->wrap_node(node ? engine->host_->last_child(node) : nullptr);
}

JSValue QuickJSScriptEngine::js_node_get_next_sibling(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                      JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    return engine->wrap_node(node ? engine->host_->next_sibling(node) : nullptr);
}

JSValue QuickJSScriptEngine::js_node_get_previous_sibling(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                          JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    return engine->wrap_node(node ? engine->host_->previous_sibling(node) : nullptr);
}

JSValue QuickJSScriptEngine::js_node_get_next_element_sibling(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                              JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    return engine->wrap_node(node ? engine->host_->next_element_sibling(node) : nullptr);
}

JSValue QuickJSScriptEngine::js_node_get_previous_element_sibling(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                                  JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    return engine->wrap_node(node ? engine->host_->previous_element_sibling(node) : nullptr);
}

JSValue QuickJSScriptEngine::js_node_get_child_nodes(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                     JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NewArray(ctx);
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_NewArray(ctx);
    return engine->wrap_node_list(engine->host_->child_nodes(node));
}

JSValue QuickJSScriptEngine::js_node_get_children(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                  JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NewArray(ctx);
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_NewArray(ctx);
    return engine->wrap_node_list(engine->host_->child_elements(node));
}

// --- Node methods ----------------------------------------------------------

JSValue QuickJSScriptEngine::js_node_append_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NULL;
    auto* parent = engine->node_from_value(this_val);
    auto* child = engine->node_from_value(argv[0]);
    return engine->wrap_node(engine->host_->append_child(parent, child));
}

JSValue QuickJSScriptEngine::js_node_insert_before(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NULL;
    auto* parent = engine->node_from_value(this_val);
    auto* child = engine->node_from_value(argv[0]);
    auto* reference = argc >= 2 ? engine->node_from_value(argv[1]) : nullptr;
    return engine->wrap_node(engine->host_->insert_before(parent, child, reference));
}

JSValue QuickJSScriptEngine::js_node_remove_child(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NULL;
    auto* parent = engine->node_from_value(this_val);
    auto* child = engine->node_from_value(argv[0]);
    return engine->wrap_node(engine->host_->remove_child(parent, child));
}

JSValue QuickJSScriptEngine::js_node_replace_child(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 2) return JS_NULL;
    auto* parent = engine->node_from_value(this_val);
    auto* new_child = engine->node_from_value(argv[0]);
    auto* old_child = engine->node_from_value(argv[1]);
    return engine->wrap_node(engine->host_->replace_child(parent, new_child, old_child));
}

JSValue QuickJSScriptEngine::js_element_set_attribute(JSContext* ctx, JSValueConst this_val, int argc,
                                                      JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 2) {
        return JS_UNDEFINED;
    }
    auto* node = engine->node_from_value(this_val);
    if (!node) {
        return JS_UNDEFINED;
    }
    const char* name = JS_ToCString(ctx, argv[0]);
    const char* value = JS_ToCString(ctx, argv[1]);
    if (name && value) {
        engine->host_->set_attribute(node, name, value);
    }
    if (name) JS_FreeCString(ctx, name);
    if (value) JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_element_get_attribute(JSContext* ctx, JSValueConst this_val, int argc,
                                                      JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) {
        return JS_NULL;
    }
    auto* node = engine->node_from_value(this_val);
    const char* name = JS_ToCString(ctx, argv[0]);
    if (!node || !name) {
        if (name) JS_FreeCString(ctx, name);
        return JS_NULL;
    }
    // Absent attribute -> null; present (even empty) -> its string value.
    JSValue result = engine->host_->has_attribute(node, name)
                         ? JS_NewString(ctx, engine->host_->get_attribute(node, name).c_str())
                         : JS_NULL;
    JS_FreeCString(ctx, name);
    return result;
}

JSValue QuickJSScriptEngine::js_element_remove_attribute(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) {
        return JS_UNDEFINED;
    }
    auto* node = engine->node_from_value(this_val);
    const char* name = JS_ToCString(ctx, argv[0]);
    if (node && name) {
        engine->host_->remove_attribute(node, name);
    }
    if (name) JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_get_class_name(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NewString(ctx, "");
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_NewString(ctx, "");
    return JS_NewString(ctx, engine->host_->get_attribute(node, "class").c_str());
}

JSValue QuickJSScriptEngine::js_node_set_class_name(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_UNDEFINED;
    const char* value = JS_ToCString(ctx, argv[0]);
    if (value) {
        engine->host_->set_attribute(node, "class", value);
        JS_FreeCString(ctx, value);
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_get_class_list(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || engine->token_list_class_id_ == 0) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_NULL;
    JSValue obj = JS_NewObjectClass(ctx, engine->token_list_class_id_);
    if (JS_IsException(obj)) return obj;
    JS_SetOpaque(obj, node);
    return obj;
}

JSValue QuickJSScriptEngine::js_node_get_dataset(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                 JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || engine->string_map_class_id_ == 0) return JS_NULL;
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_NULL;
    JSValue obj = JS_NewObjectClass(ctx, engine->string_map_class_id_);
    if (JS_IsException(obj)) return obj;
    JS_SetOpaque(obj, node);
    return obj;
}

// --- Selector queries ------------------------------------------------------

JSValue QuickJSScriptEngine::js_query_selector(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NULL;
    DOM::Node* scope = engine->node_from_value(this_val);  // null == document root
    const char* selector = JS_ToCString(ctx, argv[0]);
    if (!selector) return JS_NULL;
    DOM::Node* match = engine->host_->query_selector(scope, selector);
    JS_FreeCString(ctx, selector);
    return engine->wrap_node(match);
}

JSValue QuickJSScriptEngine::js_query_selector_all(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NewArray(ctx);
    DOM::Node* scope = engine->node_from_value(this_val);
    const char* selector = JS_ToCString(ctx, argv[0]);
    if (!selector) return JS_NewArray(ctx);
    JSValue result = engine->wrap_node_list(engine->host_->query_selector_all(scope, selector));
    JS_FreeCString(ctx, selector);
    return result;
}

JSValue QuickJSScriptEngine::js_element_matches(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_FALSE;
    DOM::Node* node = engine->node_from_value(this_val);
    const char* selector = JS_ToCString(ctx, argv[0]);
    if (!node || !selector) {
        if (selector) JS_FreeCString(ctx, selector);
        return JS_FALSE;
    }
    const bool result = engine->host_->matches(node, selector);
    JS_FreeCString(ctx, selector);
    return JS_NewBool(ctx, result ? 1 : 0);
}

JSValue QuickJSScriptEngine::js_element_closest(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NULL;
    DOM::Node* node = engine->node_from_value(this_val);
    const char* selector = JS_ToCString(ctx, argv[0]);
    if (!node || !selector) {
        if (selector) JS_FreeCString(ctx, selector);
        return JS_NULL;
    }
    DOM::Node* match = engine->host_->closest(node, selector);
    JS_FreeCString(ctx, selector);
    return engine->wrap_node(match);
}

JSValue QuickJSScriptEngine::js_get_elements_by_class_name(JSContext* ctx, JSValueConst this_val, int argc,
                                                           JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NewArray(ctx);
    DOM::Node* scope = engine->node_from_value(this_val);
    const char* names = JS_ToCString(ctx, argv[0]);
    if (!names) return JS_NewArray(ctx);
    JSValue result = engine->wrap_node_list(engine->host_->get_elements_by_class_name(scope, names));
    JS_FreeCString(ctx, names);
    return result;
}

JSValue QuickJSScriptEngine::js_get_elements_by_tag_name(JSContext* ctx, JSValueConst this_val, int argc,
                                                         JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NewArray(ctx);
    DOM::Node* scope = engine->node_from_value(this_val);
    const char* tag = JS_ToCString(ctx, argv[0]);
    if (!tag) return JS_NewArray(ctx);
    JSValue result = engine->wrap_node_list(engine->host_->get_elements_by_tag_name(scope, tag));
    JS_FreeCString(ctx, tag);
    return result;
}

// --- DOMTokenList (classList) ---------------------------------------------

JSValue QuickJSScriptEngine::js_token_list_add(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_UNDEFINED;
    auto* node = engine->node_from_opaque(this_val, engine->token_list_class_id_);
    if (!node) return JS_UNDEFINED;
    for (int i = 0; i < argc; ++i) {
        const char* token = JS_ToCString(ctx, argv[i]);
        if (token) {
            engine->host_->class_list_add(node, token);
            JS_FreeCString(ctx, token);
        }
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_token_list_remove(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_UNDEFINED;
    auto* node = engine->node_from_opaque(this_val, engine->token_list_class_id_);
    if (!node) return JS_UNDEFINED;
    for (int i = 0; i < argc; ++i) {
        const char* token = JS_ToCString(ctx, argv[i]);
        if (token) {
            engine->host_->class_list_remove(node, token);
            JS_FreeCString(ctx, token);
        }
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_token_list_toggle(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_FALSE;
    auto* node = engine->node_from_opaque(this_val, engine->token_list_class_id_);
    const char* token = JS_ToCString(ctx, argv[0]);
    if (!node || !token) {
        if (token) JS_FreeCString(ctx, token);
        return JS_FALSE;
    }
    bool present;
    if (argc >= 2) {
        // Two-arg form forces the result regardless of current membership.
        const bool force = JS_ToBool(ctx, argv[1]) != 0;
        if (force) {
            engine->host_->class_list_add(node, token);
        } else {
            engine->host_->class_list_remove(node, token);
        }
        present = force;
    } else {
        present = engine->host_->class_list_toggle(node, token);
    }
    JS_FreeCString(ctx, token);
    return JS_NewBool(ctx, present ? 1 : 0);
}

JSValue QuickJSScriptEngine::js_token_list_contains(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_FALSE;
    auto* node = engine->node_from_opaque(this_val, engine->token_list_class_id_);
    const char* token = JS_ToCString(ctx, argv[0]);
    if (!node || !token) {
        if (token) JS_FreeCString(ctx, token);
        return JS_FALSE;
    }
    const bool has = engine->host_->class_list_contains(node, token);
    JS_FreeCString(ctx, token);
    return JS_NewBool(ctx, has ? 1 : 0);
}

// --- DOMStringMap (dataset) exotic access ---------------------------------

JSValue QuickJSScriptEngine::js_string_map_get(JSContext* ctx, JSValueConst obj, JSAtom atom,
                                               JSValueConst /*receiver*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_UNDEFINED;
    auto* node = engine->node_from_opaque(obj, engine->string_map_class_id_);
    if (!node) return JS_UNDEFINED;
    const char* key = JS_AtomToCString(ctx, atom);
    if (!key) return JS_UNDEFINED;
    std::string value;
    JSValue result = engine->host_->get_dataset(node, key, value) ? JS_NewString(ctx, value.c_str()) : JS_UNDEFINED;
    JS_FreeCString(ctx, key);
    return result;
}

int QuickJSScriptEngine::js_string_map_set(JSContext* ctx, JSValueConst obj, JSAtom atom, JSValueConst value,
                                           JSValueConst /*receiver*/, int /*flags*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return -1;
    auto* node = engine->node_from_opaque(obj, engine->string_map_class_id_);
    if (!node) return -1;
    const char* key = JS_AtomToCString(ctx, atom);
    const char* val = JS_ToCString(ctx, value);
    if (node && key && val) {
        engine->host_->set_dataset(node, key, val);
    }
    if (key) JS_FreeCString(ctx, key);
    if (val) JS_FreeCString(ctx, val);
    return 1;  // report success (property assignment accepted)
}

JSValue QuickJSScriptEngine::js_native_insert_css(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                  JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->extension_host_ || argc < 2) {
        return JS_NewBool(ctx, 0);
    }

    int32_t tab_id = 0;
    if (JS_ToInt32(ctx, &tab_id, argv[0]) != 0 || tab_id < 0) {
        return JS_NewBool(ctx, 0);
    }
    const char* css_text = JS_ToCString(ctx, argv[1]);
    if (!css_text) {
        return JS_NewBool(ctx, 0);
    }

    const bool ok = engine->extension_host_->insert_css(static_cast<std::uint32_t>(tab_id), css_text);
    JS_FreeCString(ctx, css_text);
    return JS_NewBool(ctx, ok ? 1 : 0);
}

// --- Lifecycle -------------------------------------------------------------

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
    JS_NewClassID(runtime_, &node_class_id_);
    JSClassDef class_def{};
    class_def.class_name = "Node";
    JS_NewClass(runtime_, node_class_id_, &class_def);
    install_node_prototype();
    install_token_list_class();
    install_string_map_class();

    // Install console bindings unconditionally so non-DOM scripts (e.g., extensions)
    // can log without needing to bind a host.
    install_console_bindings();
}

QuickJSScriptEngine::~QuickJSScriptEngine() {
    if (context_) {
        reset_bindings();
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

void QuickJSScriptEngine::bind_extension_host(IExtensionApiHost* host) {
    ScriptEngineBase::bind_extension_host(host);
    if (!context_) {
        return;
    }
    if (host) {
        install_extension_bindings();
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

void QuickJSScriptEngine::reset_bindings() {
    if (context_) {
        for (auto& [node, value] : node_wrappers_) {
            (void)node;
            // Neutralize any wrapper the script still holds (e.g. via a global)
            // so a post-navigation access returns null instead of dereferencing
            // a node whose arena has been reset.
            JS_SetOpaque(value, nullptr);
            JS_FreeValue(context_, value);
        }
    }
    node_wrappers_.clear();
}

void QuickJSScriptEngine::install_node_prototype() {
    if (!context_) {
        return;
    }
    JSValue proto = JS_NewObject(context_);

    define_getter(proto, "nodeType", js_node_get_node_type);
    define_getter(proto, "nodeName", js_node_get_node_name);
    define_getter(proto, "tagName", js_node_get_tag_name);
    define_accessor(proto, "textContent", js_node_get_text_content, js_node_set_text_content);
    define_getter(proto, "parentNode", js_node_get_parent_node);
    define_getter(proto, "firstChild", js_node_get_first_child);
    define_getter(proto, "lastChild", js_node_get_last_child);
    define_getter(proto, "nextSibling", js_node_get_next_sibling);
    define_getter(proto, "previousSibling", js_node_get_previous_sibling);
    define_getter(proto, "nextElementSibling", js_node_get_next_element_sibling);
    define_getter(proto, "previousElementSibling", js_node_get_previous_element_sibling);
    define_getter(proto, "childNodes", js_node_get_child_nodes);
    define_getter(proto, "children", js_node_get_children);
    define_accessor(proto, "className", js_node_get_class_name, js_node_set_class_name);
    define_getter(proto, "classList", js_node_get_class_list);
    define_getter(proto, "dataset", js_node_get_dataset);

    define_method(proto, "appendChild", js_node_append_child, 1);
    define_method(proto, "insertBefore", js_node_insert_before, 2);
    define_method(proto, "removeChild", js_node_remove_child, 1);
    define_method(proto, "replaceChild", js_node_replace_child, 2);
    define_method(proto, "setAttribute", js_element_set_attribute, 2);
    define_method(proto, "getAttribute", js_element_get_attribute, 1);
    define_method(proto, "removeAttribute", js_element_remove_attribute, 1);
    define_method(proto, "querySelector", js_query_selector, 1);
    define_method(proto, "querySelectorAll", js_query_selector_all, 1);
    define_method(proto, "matches", js_element_matches, 1);
    define_method(proto, "closest", js_element_closest, 1);
    define_method(proto, "getElementsByClassName", js_get_elements_by_class_name, 1);
    define_method(proto, "getElementsByTagName", js_get_elements_by_tag_name, 1);

    // Consumes the proto reference and makes it the prototype for every wrapper.
    JS_SetClassProto(context_, node_class_id_, proto);
}

void QuickJSScriptEngine::install_token_list_class() {
    if (!context_ || !runtime_) {
        return;
    }
    JS_NewClassID(runtime_, &token_list_class_id_);
    JSClassDef class_def{};
    class_def.class_name = "DOMTokenList";
    JS_NewClass(runtime_, token_list_class_id_, &class_def);

    JSValue proto = JS_NewObject(context_);
    define_method(proto, "add", js_token_list_add, 1);
    define_method(proto, "remove", js_token_list_remove, 1);
    define_method(proto, "toggle", js_token_list_toggle, 1);
    define_method(proto, "contains", js_token_list_contains, 1);
    JS_SetClassProto(context_, token_list_class_id_, proto);
}

void QuickJSScriptEngine::install_string_map_class() {
    if (!context_ || !runtime_) {
        return;
    }
    // dataset is a live string map: property reads/writes intercept through the
    // exotic get/set handlers, which translate keys to data-* attributes.
    static JSClassExoticMethods exotic{};
    exotic.get_property = js_string_map_get;
    exotic.set_property = js_string_map_set;

    JS_NewClassID(runtime_, &string_map_class_id_);
    JSClassDef class_def{};
    class_def.class_name = "DOMStringMap";
    class_def.exotic = &exotic;
    JS_NewClass(runtime_, string_map_class_id_, &class_def);
}

void QuickJSScriptEngine::define_getter(JSValueConst proto, const char* name, JSCFunction* getter) {
    define_accessor(proto, name, getter, nullptr);
}

void QuickJSScriptEngine::define_accessor(JSValueConst proto, const char* name, JSCFunction* getter,
                                          JSCFunction* setter) {
    JSAtom atom = JS_NewAtom(context_, name);
    JSValue getter_fn = getter ? JS_NewCFunction(context_, getter, name, 0) : JS_UNDEFINED;
    JSValue setter_fn = setter ? JS_NewCFunction(context_, setter, name, 1) : JS_UNDEFINED;
    JS_DefinePropertyGetSet(context_, proto, atom, getter_fn, setter_fn, JS_PROP_CONFIGURABLE);
    JS_FreeAtom(context_, atom);
}

void QuickJSScriptEngine::define_method(JSValueConst proto, const char* name, JSCFunction* method, int length) {
    JS_SetPropertyStr(context_, proto, name, JS_NewCFunction(context_, method, name, length));
}

void QuickJSScriptEngine::install_console_bindings() {
    if (console_ready_ || !context_) {
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
    if (document_ready_ || !context_) {
        return;
    }

    JSValue global = JS_GetGlobalObject(context_);
    JSValue document = JS_NewObject(context_);
    JS_SetPropertyStr(context_, document, "getElementById",
                      JS_NewCFunction(context_, js_document_get_element_by_id, "getElementById", 1));
    JS_SetPropertyStr(context_, document, "createElement",
                      JS_NewCFunction(context_, js_document_create_element, "createElement", 1));
    JS_SetPropertyStr(context_, document, "createTextNode",
                      JS_NewCFunction(context_, js_document_create_text_node, "createTextNode", 1));
    // Document-scoped queries reuse the shared callbacks (this_val == document is
    // not a node wrapper, so the scope resolves to the document root).
    JS_SetPropertyStr(context_, document, "querySelector",
                      JS_NewCFunction(context_, js_query_selector, "querySelector", 1));
    JS_SetPropertyStr(context_, document, "querySelectorAll",
                      JS_NewCFunction(context_, js_query_selector_all, "querySelectorAll", 1));
    JS_SetPropertyStr(context_, document, "getElementsByClassName",
                      JS_NewCFunction(context_, js_get_elements_by_class_name, "getElementsByClassName", 1));
    JS_SetPropertyStr(context_, document, "getElementsByTagName",
                      JS_NewCFunction(context_, js_get_elements_by_tag_name, "getElementsByTagName", 1));
    JS_SetPropertyStr(context_, global, "document", document);
    JS_FreeValue(context_, global);
    document_ready_ = true;
}

void QuickJSScriptEngine::install_extension_bindings() {
    if (extension_ready_ || !context_) {
        return;
    }
    JSValue global = JS_GetGlobalObject(context_);
    JS_SetPropertyStr(context_, global, "__hb_nativeInsertCss",
                      JS_NewCFunction(context_, js_native_insert_css, "__hb_nativeInsertCss", 2));
    JS_FreeValue(context_, global);
    extension_ready_ = true;
}

JSValue QuickJSScriptEngine::wrap_node(DOM::Node* node) {
    if (!context_ || !node || node_class_id_ == 0) {
        return JS_NULL;
    }
    if (auto it = node_wrappers_.find(node); it != node_wrappers_.end()) {
        return JS_DupValue(context_, it->second);
    }
    JSValue obj = JS_NewObjectClass(context_, node_class_id_);
    if (JS_IsException(obj)) {
        return obj;
    }
    JS_SetOpaque(obj, node);
    // Keep one owning reference in the cache (freed on reset_bindings) so the
    // wrapper survives for the node's lifetime and identity stays stable.
    node_wrappers_.emplace(node, JS_DupValue(context_, obj));
    return obj;
}

JSValue QuickJSScriptEngine::wrap_node_list(const std::vector<DOM::Node*>& nodes) {
    JSValue array = JS_NewArray(context_);
    uint32_t index = 0;
    for (DOM::Node* node : nodes) {
        JS_SetPropertyUint32(context_, array, index++, wrap_node(node));
    }
    return array;
}

}  // namespace Hummingbird::Platform
