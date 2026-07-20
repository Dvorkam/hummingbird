#include "platform/script/QuickJSScriptEngine.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/Url.h"

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

DOM::Node* QuickJSScriptEngine::document_target() {
    return reinterpret_cast<DOM::Node*>(&document_target_marker_);
}

DOM::Node* QuickJSScriptEngine::window_target() {
    return reinterpret_cast<DOM::Node*>(&window_target_marker_);
}

DOM::Node* QuickJSScriptEngine::resolve_event_target(JSValueConst this_val) {
    // A node wrapper resolves to its node; the plain `window`/`document` objects
    // (not node wrappers) resolve to their sentinels.
    if (DOM::Node* node = node_from_value(this_val)) {
        return node;
    }
    if (JS_IsStrictEqual(context_, this_val, window_object_)) {
        return window_target();
    }
    return document_target();
}

JSValue QuickJSScriptEngine::event_target_value(DOM::Node* target) {
    if (target == window_target()) {
        return JS_DupValue(context_, window_object_);
    }
    if (target == document_target()) {
        return JS_DupValue(context_, document_object_);
    }
    return wrap_node(target);
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

// --- window.location (7.2.5) -----------------------------------------------

JSValue QuickJSScriptEngine::js_location_get_href(JSContext* ctx, JSValueConst /*this_val*/, int /*argc*/,
                                                  JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    return JS_NewString(ctx, engine ? engine->location_url_.c_str() : "");
}

JSValue QuickJSScriptEngine::js_location_get_hash(JSContext* ctx, JSValueConst /*this_val*/, int /*argc*/,
                                                  JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    const std::string hash = engine ? std::string(Core::url_fragment(engine->location_url_)) : std::string{};
    return JS_NewString(ctx, hash.c_str());
}

JSValue QuickJSScriptEngine::js_location_set_hash(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                  JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || argc < 1) return JS_UNDEFINED;
    const char* value = JS_ToCString(ctx, argv[0]);
    if (!value) return JS_UNDEFINED;
    std::string hash(value);
    JS_FreeCString(ctx, value);
    // Assigning `location.hash = "x"` normalizes to "#x" (empty clears the hash).
    if (!hash.empty() && hash.front() != '#') {
        hash.insert(hash.begin(), '#');
    }
    std::string new_url = std::string(Core::url_without_fragment(engine->location_url_));
    new_url += hash;
    if (engine->update_location(new_url)) {
        // A script-initiated fragment change: record it so the app can reflect
        // the new URL in the chrome + tab history (7.7.3).
        engine->script_location_change_ = engine->location_url_;
    }
    return JS_UNDEFINED;
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

JSValue QuickJSScriptEngine::js_node_get_inner_html(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NewString(ctx, "");
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_NewString(ctx, "");
    return JS_NewString(ctx, engine->host_->get_inner_html(node).c_str());
}

JSValue QuickJSScriptEngine::js_node_set_inner_html(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_UNDEFINED;
    const char* html = JS_ToCString(ctx, argv[0]);
    if (html) {
        engine->host_->set_inner_html(node, html);
        JS_FreeCString(ctx, html);
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_get_value(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                               JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NewString(ctx, "");
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_NewString(ctx, "");
    return JS_NewString(ctx, engine->host_->get_value(node).c_str());
}

JSValue QuickJSScriptEngine::js_node_set_value(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (!node) return JS_UNDEFINED;
    const char* value = JS_ToCString(ctx, argv[0]);
    if (value) {
        engine->host_->set_value(node, value);
        JS_FreeCString(ctx, value);
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_get_checked(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                 JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_FALSE;
    auto* node = engine->node_from_value(this_val);
    return JS_NewBool(ctx, node && engine->host_->get_checked(node) ? 1 : 0);
}

JSValue QuickJSScriptEngine::js_node_set_checked(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (node) {
        engine->host_->set_checked(node, JS_ToBool(ctx, argv[0]) != 0);
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_get_disabled(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                  JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_FALSE;
    auto* node = engine->node_from_value(this_val);
    return JS_NewBool(ctx, node && engine->host_->get_disabled(node) ? 1 : 0);
}

JSValue QuickJSScriptEngine::js_node_set_disabled(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_UNDEFINED;
    auto* node = engine->node_from_value(this_val);
    if (node) {
        engine->host_->set_disabled(node, JS_ToBool(ctx, argv[0]) != 0);
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_focus(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                           JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_UNDEFINED;
    if (auto* node = engine->node_from_value(this_val)) {
        engine->host_->set_focused(node, true);
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_blur(JSContext* ctx, JSValueConst this_val, int /*argc*/, JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_UNDEFINED;
    if (auto* node = engine->node_from_value(this_val)) {
        engine->host_->set_focused(node, false);
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
    // Fail soft (returning -1 signals an exception, which we never throw here):
    // report the assignment as handled and drop it.
    if (!engine || !engine->host_) return 1;
    auto* node = engine->node_from_opaque(obj, engine->string_map_class_id_);
    if (!node) return 1;
    const char* key = JS_AtomToCString(ctx, atom);
    const char* val = JS_ToCString(ctx, value);
    if (node && key && val) {
        engine->host_->set_dataset(node, key, val);
    }
    if (key) JS_FreeCString(ctx, key);
    if (val) JS_FreeCString(ctx, val);
    return 1;  // report success (property assignment accepted)
}

// --- EventTarget (7.2.1) ---------------------------------------------------

bool QuickJSScriptEngine::read_capture_flag(JSValueConst options) const {
    // The third argument is either a boolean capture flag or an options object
    // whose `capture` field carries it.
    if (JS_IsObject(options)) {
        JSValue capture = JS_GetPropertyStr(context_, options, "capture");
        const bool result = JS_ToBool(context_, capture) != 0;
        JS_FreeValue(context_, capture);
        return result;
    }
    return JS_ToBool(context_, options) != 0;
}

JSValue QuickJSScriptEngine::js_node_add_event_listener(JSContext* ctx, JSValueConst this_val, int argc,
                                                        JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
    DOM::Node* node = engine->resolve_event_target(this_val);
    const char* type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_UNDEFINED;
    const bool capture = argc >= 3 ? engine->read_capture_flag(argv[2]) : false;

    auto& list = engine->listeners_[node];
    // A duplicate (type, callback, capture) registration is a no-op per spec.
    for (const auto& listener : list) {
        if (listener.capture == capture && listener.type == type && JS_IsStrictEqual(ctx, listener.callback, argv[1])) {
            JS_FreeCString(ctx, type);
            return JS_UNDEFINED;
        }
    }
    list.push_back({std::string(type), JS_DupValue(ctx, argv[1]), capture});
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_node_remove_event_listener(JSContext* ctx, JSValueConst this_val, int argc,
                                                           JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || argc < 2) return JS_UNDEFINED;
    DOM::Node* node = engine->resolve_event_target(this_val);
    const char* type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_UNDEFINED;
    const bool capture = argc >= 3 ? engine->read_capture_flag(argv[2]) : false;

    if (auto it = engine->listeners_.find(node); it != engine->listeners_.end()) {
        auto& list = it->second;
        for (auto lit = list.begin(); lit != list.end(); ++lit) {
            if (lit->capture == capture && lit->type == type && JS_IsStrictEqual(ctx, lit->callback, argv[1])) {
                JS_FreeValue(ctx, lit->callback);
                list.erase(lit);
                break;
            }
        }
    }
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_event_prevent_default(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                      JSValueConst* /*argv*/) {
    JSValue cancelable = JS_GetPropertyStr(ctx, this_val, "cancelable");
    if (JS_ToBool(ctx, cancelable)) {
        JS_SetPropertyStr(ctx, this_val, "defaultPrevented", JS_TRUE);
    }
    JS_FreeValue(ctx, cancelable);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_event_stop_propagation(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                       JSValueConst* /*argv*/) {
    JS_SetPropertyStr(ctx, this_val, "__propagationStopped", JS_TRUE);
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_event_stop_immediate_propagation(JSContext* ctx, JSValueConst this_val, int /*argc*/,
                                                                 JSValueConst* /*argv*/) {
    JS_SetPropertyStr(ctx, this_val, "__propagationStopped", JS_TRUE);
    JS_SetPropertyStr(ctx, this_val, "__immediateStopped", JS_TRUE);
    return JS_UNDEFINED;
}

bool QuickJSScriptEngine::event_flag(JSValueConst event, const char* name) const {
    JSValue value = JS_GetPropertyStr(context_, event, name);
    const bool result = JS_ToBool(context_, value) != 0;
    JS_FreeValue(context_, value);
    return result;
}

JSValue QuickJSScriptEngine::make_event(const std::string& type, DOM::Node* target) {
    JSValue event = JS_NewObject(context_);
    JS_SetPropertyStr(context_, event, "type", JS_NewString(context_, type.c_str()));
    JS_SetPropertyStr(context_, event, "target", event_target_value(target));
    JS_SetPropertyStr(context_, event, "currentTarget", JS_NULL);
    JS_SetPropertyStr(context_, event, "eventPhase", JS_NewInt32(context_, 0));
    JS_SetPropertyStr(context_, event, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(context_, event, "bubbles", JS_FALSE);
    JS_SetPropertyStr(context_, event, "cancelable", JS_TRUE);
    JS_SetPropertyStr(context_, event, "key", JS_NewString(context_, ""));
    JS_SetPropertyStr(context_, event, "code", JS_NewString(context_, ""));
    JS_SetPropertyStr(context_, event, "preventDefault",
                      JS_NewCFunction(context_, js_event_prevent_default, "preventDefault", 0));
    JS_SetPropertyStr(context_, event, "stopPropagation",
                      JS_NewCFunction(context_, js_event_stop_propagation, "stopPropagation", 0));
    JS_SetPropertyStr(context_, event, "stopImmediatePropagation",
                      JS_NewCFunction(context_, js_event_stop_immediate_propagation, "stopImmediatePropagation", 0));
    return event;
}

void QuickJSScriptEngine::invoke_listeners(DOM::Node* node, const std::string& type, JSValueConst event,
                                           DispatchPhase phase) {
    auto it = listeners_.find(node);
    if (it == listeners_.end()) return;

    // Snapshot the matching callbacks (with an owned ref each) so a handler that
    // adds or removes listeners mid-dispatch cannot invalidate our iteration.
    // Capture listeners fire only in the capture phase, non-capture only in the
    // bubble phase; both fire in the target phase.
    std::vector<JSValue> to_call;
    for (const auto& listener : it->second) {
        if (listener.type != type) continue;
        if (phase == DispatchPhase::Capture && !listener.capture) continue;
        if (phase == DispatchPhase::Bubble && listener.capture) continue;
        to_call.push_back(JS_DupValue(context_, listener.callback));
    }
    if (to_call.empty()) return;

    JSValue current_target = event_target_value(node);
    JS_SetPropertyStr(context_, event, "currentTarget", JS_DupValue(context_, current_target));
    for (JSValue callback : to_call) {
        // stopImmediatePropagation halts the rest of this node's listeners.
        if (!event_flag(event, "__immediateStopped")) {
            JSValue ret = JS_Call(context_, callback, current_target, 1, &event);
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(context_);
                const char* message = JS_ToCString(context_, exc);
                HB_LOG_WARN("[js] event listener threw: " << (message ? message : "unknown"));
                if (message) JS_FreeCString(context_, message);
                JS_FreeValue(context_, exc);
            }
            JS_FreeValue(context_, ret);
        }
        JS_FreeValue(context_, callback);
    }
    JS_FreeValue(context_, current_target);
}

void QuickJSScriptEngine::dispatch_event(DOM::Node* target, const std::string& type, JSValueConst event) {
    // Propagation path: [target, parent, ..., root, document].
    std::vector<DOM::Node*> path;
    path.push_back(target);
    // Only real DOM nodes have an ancestor chain; the window/document sentinels
    // dispatch to themselves.
    if (target != document_target() && target != window_target() && host_) {
        for (DOM::Node* n = host_->parent_node(target); n; n = host_->parent_node(n)) {
            path.push_back(n);
        }
        path.push_back(document_target());
    }

    // Capture: from the document down to the target's parent (path.back()..path[1]).
    JS_SetPropertyStr(context_, event, "eventPhase", JS_NewInt32(context_, 1));
    for (size_t i = path.size(); i-- > 1;) {
        if (event_flag(event, "__propagationStopped")) break;
        invoke_listeners(path[i], type, event, DispatchPhase::Capture);
    }

    // Target phase: all listeners on the target, in registration order.
    if (!event_flag(event, "__propagationStopped")) {
        JS_SetPropertyStr(context_, event, "eventPhase", JS_NewInt32(context_, 2));
        invoke_listeners(path[0], type, event, DispatchPhase::Target);
    }

    // Bubble: from the target's parent up to the document — only if the event bubbles.
    if (event_flag(event, "bubbles")) {
        JS_SetPropertyStr(context_, event, "eventPhase", JS_NewInt32(context_, 3));
        for (size_t i = 1; i < path.size(); ++i) {
            if (event_flag(event, "__propagationStopped")) break;
            invoke_listeners(path[i], type, event, DispatchPhase::Bubble);
        }
    }

    // Post-dispatch state per spec: phase NONE, no current target. Matters for
    // handlers that stash the event object and read it later.
    JS_SetPropertyStr(context_, event, "eventPhase", JS_NewInt32(context_, 0));
    JS_SetPropertyStr(context_, event, "currentTarget", JS_NULL);
}

JSValue QuickJSScriptEngine::js_node_dispatch_event(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || argc < 1) return JS_FALSE;
    DOM::Node* target = engine->resolve_event_target(this_val);

    // Accept either a type string or an init object carrying `type` (and,
    // optionally, key/code/bubbles/cancelable — used by keyboard-event tests
    // ahead of the platform routing in 7.2.4).
    std::string type;
    JSValueConst init = JS_UNDEFINED;
    if (JS_IsObject(argv[0])) {
        init = argv[0];
        JSValue type_value = JS_GetPropertyStr(ctx, init, "type");
        if (const char* text = JS_ToCString(ctx, type_value)) {
            type = text;
            JS_FreeCString(ctx, text);
        }
        JS_FreeValue(ctx, type_value);
    } else if (const char* text = JS_ToCString(ctx, argv[0])) {
        type = text;
        JS_FreeCString(ctx, text);
    }
    if (type.empty()) return JS_FALSE;

    JSValue event = engine->make_event(type, target);
    if (JS_IsObject(init)) {
        for (const char* field : {"key", "code", "bubbles", "cancelable"}) {
            JSValue value = JS_GetPropertyStr(ctx, init, field);
            if (!JS_IsUndefined(value)) {
                JS_SetPropertyStr(ctx, event, field, value);  // takes ownership
            } else {
                JS_FreeValue(ctx, value);
            }
        }
    }

    engine->dispatch_event(target, type, event);
    const bool not_canceled = !engine->event_flag(event, "defaultPrevented");
    JS_FreeValue(ctx, event);
    return JS_NewBool(ctx, not_canceled ? 1 : 0);
}

bool QuickJSScriptEngine::dispatch_dom_event(DOM::Node* target, const ScriptDomEvent& event) {
    if (!context_ || !target) return true;
    JSValue js_event = make_event(event.type, target);
    JS_SetPropertyStr(context_, js_event, "bubbles", JS_NewBool(context_, event.bubbles ? 1 : 0));
    JS_SetPropertyStr(context_, js_event, "cancelable", JS_NewBool(context_, event.cancelable ? 1 : 0));
    if (!event.key.empty()) {
        JS_SetPropertyStr(context_, js_event, "key", JS_NewString(context_, event.key.c_str()));
    }
    if (!event.code.empty()) {
        JS_SetPropertyStr(context_, js_event, "code", JS_NewString(context_, event.code.c_str()));
    }
    dispatch_event(target, event.type, js_event);
    const bool not_canceled = !event_flag(js_event, "defaultPrevented");
    JS_FreeValue(context_, js_event);
    drain_microtasks();  // microtask checkpoint after the dispatch task (7.3.2)
    return not_canceled;
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
        JS_FreeValue(context_, document_object_);
        document_object_ = JS_UNDEFINED;
        JS_FreeValue(context_, window_object_);
        window_object_ = JS_UNDEFINED;
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
        // Even on a top-level error, promise jobs it queued before throwing run.
        drain_microtasks();
        return error_result(std::move(error));
    }
    JS_FreeValue(context_, result);
    drain_microtasks();  // microtask checkpoint after the script task (7.3.2)
    return ok_result();
}

void QuickJSScriptEngine::drain_microtasks() {
    if (!runtime_) return;
    for (;;) {
        JSContext* job_ctx = nullptr;
        int rc = JS_ExecutePendingJob(runtime_, &job_ctx);
        if (rc == 0) break;  // queue empty
        if (rc < 0 && job_ctx) {
            // A promise reaction threw: report it and keep draining the rest.
            JSValue exc = JS_GetException(job_ctx);
            const char* message = JS_ToCString(job_ctx, exc);
            HB_LOG_WARN("[js] microtask threw: " << (message ? message : "unknown"));
            if (message) JS_FreeCString(job_ctx, message);
            JS_FreeValue(job_ctx, exc);
        }
    }
}

void QuickJSScriptEngine::free_listeners() {
    if (context_) {
        for (auto& [node, list] : listeners_) {
            (void)node;
            for (auto& listener : list) {
                JS_FreeValue(context_, listener.callback);
            }
        }
    }
    listeners_.clear();
}

void QuickJSScriptEngine::reset_bindings() {
    // Drop event callbacks and timers first: neither may outlive the document,
    // and their callbacks could otherwise still reference a wrapper we are about
    // to free.
    free_listeners();
    free_timers();
    free_animation_frames();
    missing_apis_.clear();            // telemetry is per-document (7.5.2)
    script_location_change_.reset();  // per-document location state (7.7.3)
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
    define_accessor(proto, "innerHTML", js_node_get_inner_html, js_node_set_inner_html);
    define_accessor(proto, "value", js_node_get_value, js_node_set_value);
    define_accessor(proto, "checked", js_node_get_checked, js_node_set_checked);
    define_accessor(proto, "disabled", js_node_get_disabled, js_node_set_disabled);
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
    define_method(proto, "focus", js_node_focus, 0);
    define_method(proto, "blur", js_node_blur, 0);
    define_method(proto, "addEventListener", js_node_add_event_listener, 3);
    define_method(proto, "removeEventListener", js_node_remove_event_listener, 3);
    define_method(proto, "dispatchEvent", js_node_dispatch_event, 1);

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
    // document is an EventTarget: listeners registered here catch events that
    // bubble to the top (e.g. hn.js delegates clicks on `document`). The shared
    // callbacks resolve `this` == document to the document sentinel.
    JS_SetPropertyStr(context_, document, "addEventListener",
                      JS_NewCFunction(context_, js_node_add_event_listener, "addEventListener", 3));
    JS_SetPropertyStr(context_, document, "removeEventListener",
                      JS_NewCFunction(context_, js_node_remove_event_listener, "removeEventListener", 3));
    JS_SetPropertyStr(context_, document, "dispatchEvent",
                      JS_NewCFunction(context_, js_node_dispatch_event, "dispatchEvent", 1));
    // Retain a reference so the event machinery can use `document` as an
    // EventTarget value (target / currentTarget / `this`).
    document_object_ = JS_DupValue(context_, document);
    JS_SetPropertyStr(context_, global, "document", document);
    JS_FreeValue(context_, global);
    document_ready_ = true;

    install_window_bindings();
}

void QuickJSScriptEngine::install_window_bindings() {
    if (!context_ || !JS_IsUndefined(window_object_)) {
        return;
    }
    JSValue global = JS_GetGlobalObject(context_);

    // location: href (read) + hash (read/write; assigning fires hashchange).
    JSValue location = JS_NewObject(context_);
    define_accessor(location, "href", js_location_get_href, nullptr);
    define_accessor(location, "hash", js_location_get_hash, js_location_set_hash);

    // window is an EventTarget (hashchange fires here) and owns location.
    JSValue window = JS_NewObject(context_);
    JS_SetPropertyStr(context_, window, "location", JS_DupValue(context_, location));
    JS_SetPropertyStr(context_, window, "addEventListener",
                      JS_NewCFunction(context_, js_node_add_event_listener, "addEventListener", 3));
    JS_SetPropertyStr(context_, window, "removeEventListener",
                      JS_NewCFunction(context_, js_node_remove_event_listener, "removeEventListener", 3));
    JS_SetPropertyStr(context_, window, "dispatchEvent",
                      JS_NewCFunction(context_, js_node_dispatch_event, "dispatchEvent", 1));
    // Timers (7.3.1): available as both window.* and bare globals.
    JS_SetPropertyStr(context_, window, "setTimeout", JS_NewCFunction(context_, js_set_timeout, "setTimeout", 2));
    JS_SetPropertyStr(context_, window, "setInterval", JS_NewCFunction(context_, js_set_interval, "setInterval", 2));
    JS_SetPropertyStr(context_, window, "clearTimeout", JS_NewCFunction(context_, js_clear_timer, "clearTimeout", 1));
    JS_SetPropertyStr(context_, window, "clearInterval", JS_NewCFunction(context_, js_clear_timer, "clearInterval", 1));
    JS_SetPropertyStr(context_, window, "requestAnimationFrame",
                      JS_NewCFunction(context_, js_request_animation_frame, "requestAnimationFrame", 1));
    JS_SetPropertyStr(context_, window, "cancelAnimationFrame",
                      JS_NewCFunction(context_, js_cancel_animation_frame, "cancelAnimationFrame", 1));
    window_object_ = JS_DupValue(context_, window);

    // Both `window` and bare `location` are global (window.location === location).
    JS_SetPropertyStr(context_, global, "location", location);  // transfers the ref
    JS_SetPropertyStr(context_, global, "window", window);      // transfers the ref
    JS_SetPropertyStr(context_, global, "setTimeout", JS_NewCFunction(context_, js_set_timeout, "setTimeout", 2));
    JS_SetPropertyStr(context_, global, "setInterval", JS_NewCFunction(context_, js_set_interval, "setInterval", 2));
    JS_SetPropertyStr(context_, global, "clearTimeout", JS_NewCFunction(context_, js_clear_timer, "clearTimeout", 1));
    JS_SetPropertyStr(context_, global, "clearInterval", JS_NewCFunction(context_, js_clear_timer, "clearInterval", 1));
    JS_SetPropertyStr(context_, global, "requestAnimationFrame",
                      JS_NewCFunction(context_, js_request_animation_frame, "requestAnimationFrame", 1));
    JS_SetPropertyStr(context_, global, "cancelAnimationFrame",
                      JS_NewCFunction(context_, js_cancel_animation_frame, "cancelAnimationFrame", 1));
    JS_FreeValue(context_, global);

    install_failsoft_stubs();  // fail-soft stubs for unimplemented APIs (7.5.2)
}

void QuickJSScriptEngine::set_location(std::string_view url) {
    location_url_ = std::string(url);
    // App-initiated: the caller already knows this URL, so drop any pending
    // script-initiated change (a stale value from the previous document).
    script_location_change_.reset();
}

bool QuickJSScriptEngine::navigate_fragment(std::string_view url) {
    // App-initiated fragment nav: fire hashchange but do not report it back as a
    // script change (the app is the one driving it).
    bool changed = update_location(url);
    script_location_change_.reset();
    return changed;
}

std::optional<std::string> QuickJSScriptEngine::consume_location_change() {
    std::optional<std::string> out = std::move(script_location_change_);
    script_location_change_.reset();
    return out;
}

bool QuickJSScriptEngine::update_location(std::string_view url) {
    const std::string old_url = location_url_;
    const bool hash_changed = Core::url_fragment(old_url) != Core::url_fragment(url);
    location_url_ = std::string(url);
    if (!hash_changed || !context_) {
        return false;
    }
    // Same-document fragment change: fire hashchange on window (no reload).
    JSValue event = make_event("hashchange", window_target());
    JS_SetPropertyStr(context_, event, "oldURL", JS_NewString(context_, old_url.c_str()));
    JS_SetPropertyStr(context_, event, "newURL", JS_NewString(context_, location_url_.c_str()));
    dispatch_event(window_target(), "hashchange", event);
    JS_FreeValue(context_, event);
    drain_microtasks();  // checkpoint after hashchange listeners (7.3.2)
    return true;
}

JSValue QuickJSScriptEngine::js_set_timeout(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine) return JS_NewInt64(ctx, 0);
    return engine->add_timer(argc, argv, /*repeating=*/false);
}

JSValue QuickJSScriptEngine::js_set_interval(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine) return JS_NewInt64(ctx, 0);
    return engine->add_timer(argc, argv, /*repeating=*/true);
}

JSValue QuickJSScriptEngine::js_clear_timer(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (engine && argc >= 1) {
        int64_t id = 0;
        if (JS_ToInt64(ctx, &id, argv[0]) == 0) {
            engine->remove_timer(id);
        }
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::add_timer(int argc, JSValueConst* argv, bool repeating) {
    if (!context_) return JS_NewInt64(context_, 0);
    if (argc < 1 || !JS_IsFunction(context_, argv[0])) {
        // Fail-soft: a non-callable handler is ignored rather than throwing.
        HB_LOG_WARN("[js] setTimeout/setInterval ignored: handler is not a function");
        return JS_NewInt64(context_, 0);
    }

    double delay_ms = 0.0;
    if (argc >= 2 && JS_ToFloat64(context_, &delay_ms, argv[1]) < 0) {
        JS_FreeValue(context_, JS_GetException(context_));  // treat an unconvertible delay as 0
        delay_ms = 0.0;
    }
    if (!(delay_ms >= 0.0)) delay_ms = 0.0;  // clamp negatives and NaN

    Timer timer;
    const int64_t id = next_timer_id_++;
    timer.id = id;
    timer.callback = JS_DupValue(context_, argv[0]);
    for (int i = 2; i < argc; ++i) {
        timer.args.push_back(JS_DupValue(context_, argv[i]));
    }
    timer.interval_ms = delay_ms;
    timer.fire_at_ms = now_ms_ + delay_ms;
    timer.repeating = repeating;
    timers_.push_back(std::move(timer));
    return JS_NewInt64(context_, id);
}

void QuickJSScriptEngine::remove_timer(int64_t id) {
    auto it = std::find_if(timers_.begin(), timers_.end(), [&](const Timer& t) { return t.id == id; });
    if (it == timers_.end()) return;
    if (context_) {
        JS_FreeValue(context_, it->callback);
        for (JSValue arg : it->args) JS_FreeValue(context_, arg);
    }
    timers_.erase(it);
}

void QuickJSScriptEngine::free_timers() {
    if (context_) {
        for (auto& timer : timers_) {
            JS_FreeValue(context_, timer.callback);
            for (JSValue arg : timer.args) JS_FreeValue(context_, arg);
        }
    }
    timers_.clear();
    next_timer_id_ = 1;
    now_ms_ = 0.0;
}

bool QuickJSScriptEngine::run_due_timers(double now_ms) {
    if (!context_) return false;
    now_ms_ = now_ms;

    // Snapshot the due timers in fire order (deadline, then registration id) so a
    // callback that (re)schedules or clears timers cannot disturb this pass; any
    // new timer it adds runs on a later tick, which avoids a same-tick storm from
    // a zero-delay setInterval.
    struct DueRef {
        double fire_at;
        int64_t id;
    };
    std::vector<DueRef> due;
    for (const auto& timer : timers_) {
        if (timer.fire_at_ms <= now_ms_) due.push_back({timer.fire_at_ms, timer.id});
    }
    std::sort(due.begin(), due.end(), [](const DueRef& a, const DueRef& b) {
        return a.fire_at != b.fire_at ? a.fire_at < b.fire_at : a.id < b.id;
    });

    bool fired = false;
    for (const auto& ref : due) {
        // Re-find by id: an earlier callback in this pass may have cleared it.
        auto it = std::find_if(timers_.begin(), timers_.end(), [&](const Timer& t) { return t.id == ref.id; });
        if (it == timers_.end()) continue;

        // Own copies of the callback + args so a re-entrant clear during the call
        // cannot free them out from under JS_Call.
        JSValue callback = JS_DupValue(context_, it->callback);
        std::vector<JSValue> call_args;
        call_args.reserve(it->args.size());
        for (JSValue arg : it->args) call_args.push_back(JS_DupValue(context_, arg));

        // Reschedule (interval) or drop (one-shot) BEFORE invoking, so a
        // clearInterval(self) inside the callback still wins.
        if (it->repeating) {
            it->fire_at_ms = now_ms_ + it->interval_ms;
        } else {
            JS_FreeValue(context_, it->callback);
            for (JSValue arg : it->args) JS_FreeValue(context_, arg);
            timers_.erase(it);
        }

        JSValue ret = JS_Call(context_, callback, JS_UNDEFINED, static_cast<int>(call_args.size()), call_args.data());
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(context_);
            const char* message = JS_ToCString(context_, exc);
            HB_LOG_WARN("[js] timer callback threw: " << (message ? message : "unknown"));
            if (message) JS_FreeCString(context_, message);
            JS_FreeValue(context_, exc);
        }
        JS_FreeValue(context_, ret);
        JS_FreeValue(context_, callback);
        for (JSValue arg : call_args) JS_FreeValue(context_, arg);
        // Each timer callback is its own task: drain microtasks before the next.
        drain_microtasks();
        fired = true;
    }
    return fired;
}

JSValue QuickJSScriptEngine::js_request_animation_frame(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->context_) return JS_NewInt64(ctx, 0);
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_NewInt64(ctx, 0);  // fail-soft: ignore a non-callable request
    }
    const int64_t id = engine->next_raf_id_++;
    engine->animation_frames_.push_back({id, JS_DupValue(engine->context_, argv[0])});
    return JS_NewInt64(ctx, id);
}

JSValue QuickJSScriptEngine::js_cancel_animation_frame(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (engine && engine->context_ && argc >= 1) {
        int64_t id = 0;
        if (JS_ToInt64(ctx, &id, argv[0]) == 0) {
            auto& list = engine->animation_frames_;
            auto it = std::find_if(list.begin(), list.end(), [&](const auto& c) { return c.id == id; });
            if (it != list.end()) {
                JS_FreeValue(engine->context_, it->callback);
                list.erase(it);
            }
        }
    }
    return JS_UNDEFINED;
}

bool QuickJSScriptEngine::run_animation_frames(double now_ms) {
    if (!context_ || animation_frames_.empty()) return false;
    // Snapshot this frame's callbacks and clear the queue up front, so a callback
    // that re-requests rAF registers for the NEXT frame — one callback per frame,
    // no queue growth.
    std::vector<AnimationFrameCallback> frame = std::move(animation_frames_);
    animation_frames_.clear();

    JSValue arg = JS_NewFloat64(context_, now_ms);
    for (auto& cb : frame) {
        JSValue ret = JS_Call(context_, cb.callback, JS_UNDEFINED, 1, &arg);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(context_);
            const char* message = JS_ToCString(context_, exc);
            HB_LOG_WARN("[js] requestAnimationFrame callback threw: " << (message ? message : "unknown"));
            if (message) JS_FreeCString(context_, message);
            JS_FreeValue(context_, exc);
        }
        JS_FreeValue(context_, ret);
        JS_FreeValue(context_, cb.callback);
        drain_microtasks();  // microtask checkpoint after the callback (7.3.2)
    }
    JS_FreeValue(context_, arg);
    return true;
}

void QuickJSScriptEngine::free_animation_frames() {
    if (context_) {
        for (auto& cb : animation_frames_) JS_FreeValue(context_, cb.callback);
    }
    animation_frames_.clear();
    next_raf_id_ = 1;
}

JSValue QuickJSScriptEngine::js_report_missing_api(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (engine && argc >= 1) {
        const char* name = JS_ToCString(ctx, argv[0]);
        if (name) {
            engine->record_missing_api(name);
            JS_FreeCString(ctx, name);
        }
    }
    return JS_UNDEFINED;
}

void QuickJSScriptEngine::record_missing_api(std::string name) {
    if (name.empty()) return;
    if (std::find(missing_apis_.begin(), missing_apis_.end(), name) != missing_apis_.end()) {
        return;  // already reported this page — dedupe
    }
    HB_LOG_INFO("[js] unimplemented API touched (fail-soft): " << name);
    missing_apis_.push_back(std::move(name));
}

void QuickJSScriptEngine::install_failsoft_stubs() {
    if (failsoft_ready_ || !context_) {
        return;
    }
    JSValue global = JS_GetGlobalObject(context_);
    JS_SetPropertyStr(context_, global, "__hb_reportMissingApi",
                      JS_NewCFunction(context_, js_report_missing_api, "__hb_reportMissingApi", 1));
    JS_FreeValue(context_, global);

    // Define benign stand-ins for high-value APIs a page may touch that we do not
    // implement yet. Each is guarded by `typeof === 'undefined'`, so when the real
    // API lands later it wins; each reports once (record_missing_api dedupes) and
    // returns a no-op instead of throwing, so the rest of the script keeps running.
    static constexpr const char* kPrelude = R"JS(
(function (g) {
  var report = g.__hb_reportMissingApi;
  function store(name) {
    return {
      getItem: function () { report(name); return null; },
      setItem: function () { report(name); },
      removeItem: function () { report(name); },
      clear: function () { report(name); },
      key: function () { report(name); return null; },
      length: 0
    };
  }
  if (typeof g.fetch === 'undefined') {
    g.fetch = function () { report('fetch'); return new Promise(function () {}); };
  }
  if (typeof g.XMLHttpRequest === 'undefined') {
    g.XMLHttpRequest = function () { report('XMLHttpRequest'); };
    g.XMLHttpRequest.prototype.open = function () {};
    g.XMLHttpRequest.prototype.send = function () {};
    g.XMLHttpRequest.prototype.abort = function () {};
    g.XMLHttpRequest.prototype.setRequestHeader = function () {};
    g.XMLHttpRequest.prototype.getResponseHeader = function () { return null; };
    g.XMLHttpRequest.prototype.addEventListener = function () {};
    g.XMLHttpRequest.prototype.removeEventListener = function () {};
  }
  if (typeof g.localStorage === 'undefined') { g.localStorage = store('localStorage'); }
  if (typeof g.sessionStorage === 'undefined') { g.sessionStorage = store('sessionStorage'); }
  if (typeof g.matchMedia === 'undefined') {
    g.matchMedia = function () {
      report('matchMedia');
      return { matches: false, media: '', addListener: function () {}, removeListener: function () {},
               addEventListener: function () {}, removeEventListener: function () {} };
    };
  }
  // Minimal, real (not fail-soft) URL + URLSearchParams. Enough for pages that
  // parse link hrefs — e.g. Hacker News' hn.js does `new URL(el.href, location)`
  // in its delegated click handler before deciding vote/hide/collapse. Never
  // throws: unparseable input yields an opaque pathname so the caller's routing
  // still runs. Not spec-complete (opaque vs special schemes, IDNA, etc.).
  function makeSearchParams(qs) {
    var pairs = [];
    if (qs) {
      var parts = qs.split('&');
      for (var i = 0; i < parts.length; i++) {
        if (!parts[i]) continue;
        var eq = parts[i].indexOf('=');
        var k = eq < 0 ? parts[i] : parts[i].slice(0, eq);
        var v = eq < 0 ? '' : parts[i].slice(eq + 1);
        try { k = decodeURIComponent(k.replace(/\+/g, ' ')); } catch (e) {}
        try { v = decodeURIComponent(v.replace(/\+/g, ' ')); } catch (e) {}
        pairs.push([k, v]);
      }
    }
    return {
      get: function (name) {
        for (var i = 0; i < pairs.length; i++) if (pairs[i][0] === name) return pairs[i][1];
        return null;
      },
      has: function (name) {
        for (var i = 0; i < pairs.length; i++) if (pairs[i][0] === name) return true;
        return false;
      },
      toString: function () { return qs || ''; }
    };
  }
  function URLImpl(url, base) {
    url = String(url);
    var abs = /^[a-zA-Z][a-zA-Z0-9+.\-]*:/.test(url);
    var full = url;
    if (!abs && base != null) {
      base = String(base);
      var bm = base.match(/^([a-zA-Z][a-zA-Z0-9+.\-]*:\/\/[^\/?#]*)([^?#]*)/);
      var origin = bm ? bm[1] : '';
      var path = bm ? (bm[2] || '/') : '/';
      if (url.charAt(0) === '/') full = origin + url;
      else if (url.charAt(0) === '#' || url.charAt(0) === '?') full = origin + path + url;
      else full = origin + path.replace(/[^\/]*$/, '') + url;
    }
    var m = full.match(/^([a-zA-Z][a-zA-Z0-9+.\-]*:)(\/\/([^\/?#]*))?([^?#]*)(\?[^#]*)?(#.*)?$/);
    this.href = full;
    if (m) {
      this.protocol = m[1];
      this.host = m[3] || '';
      this.hostname = this.host.split(':')[0];
      this.pathname = m[4] || '';
      this.search = m[5] || '';
      this.hash = m[6] || '';
    } else {
      this.protocol = ''; this.host = ''; this.hostname = '';
      this.pathname = full; this.search = ''; this.hash = '';
    }
    this.searchParams = makeSearchParams(this.search.replace(/^\?/, ''));
  }
  if (typeof g.URL === 'undefined') { g.URL = URLImpl; }
  if (typeof g.URLSearchParams === 'undefined') {
    g.URLSearchParams = function (init) { return makeSearchParams(String(init == null ? '' : init).replace(/^\?/, '')); };
  }
})(globalThis);
)JS";
    JSValue result =
        JS_Eval(context_, kPrelude, std::char_traits<char>::length(kPrelude), "<failsoft-stubs>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(context_);
        const char* message = JS_ToCString(context_, exc);
        HB_LOG_WARN("[js] fail-soft stub install failed: " << (message ? message : "unknown"));
        if (message) JS_FreeCString(context_, message);
        JS_FreeValue(context_, exc);
    }
    JS_FreeValue(context_, result);
    failsoft_ready_ = true;
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
