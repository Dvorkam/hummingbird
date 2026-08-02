#include "platform/script/QuickJSScriptEngine.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/net/Origin.h"
#include "core/utils/Log.h"
#include "core/utils/Url.h"

// The QuickJS engine is a thin adapter: it holds DOM nodes only as opaque
// handles and performs every read/mutation through IScriptHost. The core/dom
// includes above exist solely so the compiler knows Element derives from Node
// (needed to wrap an Element* as a Node*); no DOM methods are called here.

namespace Hummingbird::Platform {

namespace {
// RAII: brackets one JS entry point (a script eval, an event dispatch, a single
// timer or animation-frame callback) so the engine can tell a nested entry from
// the outermost one. Only the outermost may run the microtask checkpoint
// (T-DISPATCH-MICROTASK-REENTRANT-1, story 9.0.1).
class ScriptEntryScope {
public:
    explicit ScriptEntryScope(int& depth) : depth_(depth) { ++depth_; }
    ~ScriptEntryScope() {
        if (depth_ > 0) --depth_;
    }
    ScriptEntryScope(const ScriptEntryScope&) = delete;
    ScriptEntryScope& operator=(const ScriptEntryScope&) = delete;

private:
    int& depth_;
};
}  // namespace

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
    // An unqualified call — `addEventListener(...)` with no base object — arrives
    // with `this` undefined, because quickjs does not substitute the global for a
    // C function the way sloppy-mode JS does for a script function. A bare
    // addEventListener IS window.addEventListener, so it must not fall through to
    // the document (where its listener would never see a window event).
    if (JS_IsUndefined(this_val) || JS_IsNull(this_val)) {
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

JSValue QuickJSScriptEngine::js_console_log(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv,
                                            int magic) {
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
    // Severity is carried in `magic` so a page's own console.error survives a
    // build that only logs errors — routing everything through INFO meant the
    // most important half of a page's diagnostics vanished first.
    switch (static_cast<ConsoleLevel>(magic)) {
        case ConsoleLevel::Warn:
            HB_LOG_WARN("[js] " << message);
            break;
        case ConsoleLevel::Error:
            HB_LOG_ERROR("[js] " << message);
            break;
        case ConsoleLevel::Info:
        default:
            HB_LOG_INFO("[js] " << message);
            break;
    }
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

// document.documentElement / body / head (T-DOM-DOCUMENT-BODY-1). `magic`
// selects which, so all three share one callback.
//
// Returns null rather than throwing when the document has no such element —
// which is what a browser does, and what lets a page's own `if (document.body)`
// guard work. The throw pages actually hit is on the *use* of the null, at the
// point the mistake is.
JSValue QuickJSScriptEngine::js_document_get_part(JSContext* ctx, JSValueConst /*this_val*/, int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) {
        return JS_NULL;
    }
    return engine->wrap_node(engine->host_->document_part(static_cast<IScriptHost::DocumentPart>(magic)));
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

// document.cookie (8.1.5). The getter returns only what script may see: the jar
// withholds HttpOnly cookies from this path, which is the flag's entire purpose.
JSValue QuickJSScriptEngine::js_document_get_cookie(JSContext* ctx, JSValueConst /*this_val*/, int /*argc*/,
                                                    JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) {
        return JS_NewString(ctx, "");
    }
    return JS_NewString(ctx, engine->host_->get_document_cookie().c_str());
}

// Assigning document.cookie sets ONE cookie; it does not replace the jar. The
// string is parsed by the same code as a server's Set-Cookie, so attribute
// handling cannot drift between the two paths.
JSValue QuickJSScriptEngine::js_document_set_cookie(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                    JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) {
        return JS_UNDEFINED;
    }
    const char* value = JS_ToCString(ctx, argv[0]);
    if (!value) {
        return JS_UNDEFINED;
    }
    engine->host_->set_document_cookie(value);
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

// window.localStorage (8.2.2) and window.sessionStorage (8.2.3). All six route
// through the host to the StorageArea for the current origin; the host degrades
// to empty/no-op when there is no store (opaque origin, or no store), so these
// never need a guard beyond host_ itself. Web Storage coerces both key and value
// to strings, which JS_ToCString does here.
//
// `magic` selects which of the two areas: 0 = localStorage, 1 = sessionStorage.
// The two objects install the same six functions with different magic, so the
// binding code exists once.
static IScriptHost::StorageKind storage_kind_of(int magic) {
    return magic == 1 ? IScriptHost::StorageKind::Session : IScriptHost::StorageKind::Local;
}

JSValue QuickJSScriptEngine::js_storage_get_item(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                 JSValueConst* argv, int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NULL;
    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_NULL;
    auto value = engine->host_->storage_get_item(storage_kind_of(magic), key);
    JS_FreeCString(ctx, key);
    // getItem returns null (not undefined) for a missing key, per spec.
    return value ? JS_NewString(ctx, value->c_str()) : JS_NULL;
}

JSValue QuickJSScriptEngine::js_storage_set_item(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                 JSValueConst* argv, int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 2) return JS_UNDEFINED;
    const char* key = JS_ToCString(ctx, argv[0]);
    const char* value = JS_ToCString(ctx, argv[1]);
    IScriptHost::StorageWriteResult result = IScriptHost::StorageWriteResult::Ok;
    if (key && value) {
        result = engine->host_->storage_set_item(storage_kind_of(magic), key, value);
    }
    if (key) JS_FreeCString(ctx, key);
    if (value) JS_FreeCString(ctx, value);
    if (result == IScriptHost::StorageWriteResult::QuotaExceeded) {
        // The spec throws a DOMException named QuotaExceededError; pages check
        // e.name, so build an Error carrying that name rather than a bare
        // TypeError. JS_Throw returns JS_EXCEPTION.
        JSValue error = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, error, "name", JS_NewString(ctx, "QuotaExceededError"));
        JS_SetPropertyStr(ctx, error, "message",
                          JS_NewString(ctx, "Failed to execute 'setItem' on 'Storage': quota exceeded."));
        return JS_Throw(ctx, error);
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_storage_remove_item(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                    JSValueConst* argv, int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_UNDEFINED;
    const char* key = JS_ToCString(ctx, argv[0]);
    if (key) {
        engine->host_->storage_remove_item(storage_kind_of(magic), key);
        JS_FreeCString(ctx, key);
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_storage_clear(JSContext* ctx, JSValueConst /*this_val*/, int /*argc*/,
                                              JSValueConst* /*argv*/, int magic) {
    auto* engine = engine_from_context(ctx);
    if (engine && engine->host_) {
        engine->host_->storage_clear(storage_kind_of(magic));
    }
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_storage_key(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv,
                                            int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_ || argc < 1) return JS_NULL;
    uint32_t index = 0;
    if (JS_ToUint32(ctx, &index, argv[0]) != 0) return JS_NULL;
    auto key = engine->host_->storage_key(storage_kind_of(magic), index);
    return key ? JS_NewString(ctx, key->c_str()) : JS_NULL;
}

// A getter-with-magic: QuickJS calls this as (ctx, this_val, magic), so it takes
// no argc/argv unlike the five method functions above.
JSValue QuickJSScriptEngine::js_storage_get_length(JSContext* ctx, JSValueConst /*this_val*/, int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->host_) return JS_NewInt32(ctx, 0);
    return JS_NewInt32(ctx, static_cast<int32_t>(engine->host_->storage_length(storage_kind_of(magic))));
}

JSValue QuickJSScriptEngine::js_location_get_href(JSContext* ctx, JSValueConst /*this_val*/, int /*argc*/,
                                                  JSValueConst* /*argv*/) {
    auto* engine = engine_from_context(ctx);
    return JS_NewString(ctx, engine ? engine->location_url_.c_str() : "");
}

// The URL components a page reads off `location`. Only `href` and `hash` were
// bound, so `location.pathname` — which routing code reaches for constantly —
// was undefined, and calling `.split()` on it threw. Same shape of gap as
// `document.body` and `window.console`: the object existed, the members did not.
//
// Read-only. Assigning to `location.href`/`pathname` NAVIGATES, which needs the
// Tab, and is filed as T-JS-LOCATION-NAVIGATE-1 rather than half-done here.
JSValue QuickJSScriptEngine::js_location_get_part(JSContext* ctx, JSValueConst /*this_val*/, int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine) {
        return JS_NewString(ctx, "");
    }
    const std::string& url = engine->location_url_;
    auto parts = Core::parse_absolute_url(url);
    if (!parts) {
        return JS_NewString(ctx, "");
    }

    // `UrlParts::path` runs from the first '/' to the end, so it still carries
    // the query and fragment; pathname and search are cut out of it here.
    std::string_view path = parts->path;
    const size_t fragment_pos = path.find('#');
    if (fragment_pos != std::string_view::npos) {
        path = path.substr(0, fragment_pos);
    }
    std::string_view pathname = path;
    std::string_view search;
    if (const size_t query_pos = path.find('?'); query_pos != std::string_view::npos) {
        pathname = path.substr(0, query_pos);
        search = path.substr(query_pos);
    }

    const std::string port = parts->port ? std::to_string(*parts->port) : std::string();
    const std::string host_with_port = port.empty() ? parts->host : parts->host + ":" + port;

    switch (static_cast<LocationPart>(magic)) {
        case LocationPart::Protocol:
            return JS_NewString(ctx, (parts->scheme + ":").c_str());  // includes the colon, per spec
        case LocationPart::Host:
            return JS_NewString(ctx, host_with_port.c_str());
        case LocationPart::Hostname:
            return JS_NewString(ctx, parts->host.c_str());
        case LocationPart::Port:
            // Empty for a default port, which is what a browser reports.
            return JS_NewString(ctx, port.c_str());
        case LocationPart::Pathname:
            return JS_NewStringLen(ctx, pathname.data(), pathname.size());
        case LocationPart::Search:
            return JS_NewStringLen(ctx, search.data(), search.size());
        case LocationPart::Origin:
            return JS_NewString(ctx, (parts->scheme + "://" + host_with_port).c_str());
    }
    return JS_NewString(ctx, "");
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
    // On Event.prototype, so an engine-dispatched event and a page-constructed
    // one answer `instanceof Event` the same way. Two sources of events that
    // disagree about their own type would be worse than having no constructor.
    JSValue event = JS_IsUndefined(event_proto_) ? JS_NewObject(context_) : JS_NewObjectProto(context_, event_proto_);
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
        // `detail` is what a CustomEvent exists to carry — dropping it would
        // deliver the event and lose the only thing the sender put in it.
        for (const char* field : {"key", "code", "bubbles", "cancelable", "detail"}) {
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
    {
        ScriptEntryScope entry(script_entry_depth_);
        dispatch_event(target, event.type, js_event);
    }
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

    // The identity comes from the ENGINE, not from an argument: script inside
    // the context cannot name a different extension than the one it belongs to.
    const bool ok =
        engine->extension_host_->insert_css(engine->extension_id_, static_cast<std::uint32_t>(tab_id), css_text);
    JS_FreeCString(ctx, css_text);
    return JS_NewBool(ctx, ok ? 1 : 0);
}

// Declarative request filtering (9.4.1). The rules arrive as a JSON STRING
// rather than as a JS object graph, so the host parses exactly the same text a
// manifest ruleset contains — one format, one parser, one set of rejection
// rules. Walking a QuickJS object here would be a second implementation that
// could disagree with the file one about what a valid rule is.
JSValue QuickJSScriptEngine::js_native_set_filter_rules(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                        JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    if (!engine || !engine->extension_host_ || argc < 1) {
        return JS_NewBool(ctx, 0);
    }
    const char* rules_json = JS_ToCString(ctx, argv[0]);
    if (!rules_json) {
        return JS_NewBool(ctx, 0);
    }
    const bool ok = engine->extension_host_->set_filter_rules(engine->extension_id_, rules_json);
    JS_FreeCString(ctx, rules_json);
    return JS_NewBool(ctx, ok ? 1 : 0);
}

// --- fetch (9.1.1) ---------------------------------------------------------

JSValue QuickJSScriptEngine::js_native_fetch(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* engine = engine_from_context(ctx);
    // A promise capability is created up front in every path, so even a rejection
    // is delivered asynchronously — fetch must never throw synchronously at its
    // caller for a bad argument or a missing host.
    JSValue functions[2];
    JSValue promise = JS_NewPromiseCapability(ctx, functions);
    if (JS_IsException(promise)) {
        return promise;
    }
    const auto settle_now = [&](bool resolve, JSValue value) {
        JSValue ret = JS_Call(ctx, functions[resolve ? 0 : 1], JS_UNDEFINED, 1, &value);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, functions[0]);
        JS_FreeValue(ctx, functions[1]);
        return promise;
    };
    const auto reject_with = [&](const char* message) {
        JSValue error = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
        return settle_now(/*resolve=*/false, error);
    };

    if (!engine || !engine->host_) {
        return reject_with("fetch is unavailable: no network for this document");
    }
    if (argc < 1) {
        return reject_with("fetch requires a URL");
    }

    ScriptFetchRequest request;
    if (const char* url = JS_ToCString(ctx, argv[0]); url) {
        // Relative URLs are resolved by the host: only the engine knows the
        // document's base.
        request.url = engine->host_->resolve_url(url);
        JS_FreeCString(ctx, url);
    }
    if (request.url.empty()) {
        return reject_with("fetch requires a URL");
    }

    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue method = JS_GetPropertyStr(ctx, argv[1], "method");
        if (JS_IsString(method)) {
            if (const char* text = JS_ToCString(ctx, method); text) {
                request.method = Core::Utils::to_upper(text);
                JS_FreeCString(ctx, text);
            }
        }
        JS_FreeValue(ctx, method);

        JSValue body = JS_GetPropertyStr(ctx, argv[1], "body");
        if (!JS_IsUndefined(body) && !JS_IsNull(body)) {
            if (const char* text = JS_ToCString(ctx, body); text) {
                request.body = text;
                request.has_body = true;
                JS_FreeCString(ctx, text);
            }
        }
        JS_FreeValue(ctx, body);

        // Headers as a plain object; the prelude normalizes a Headers instance
        // or an array of pairs into one before calling in.
        // credentials: 'omit' | 'same-origin' (default) | 'include' (9.2.1).
        JSValue credentials = JS_GetPropertyStr(ctx, argv[1], "credentials");
        if (JS_IsString(credentials)) {
            if (const char* text = JS_ToCString(ctx, credentials); text) {
                const std::string mode = Core::Utils::to_lower(text);
                if (mode == "omit") {
                    request.credentials = Core::Cors::Credentials::Omit;
                } else if (mode == "include") {
                    request.credentials = Core::Cors::Credentials::Include;
                }
                JS_FreeCString(ctx, text);
            }
        }
        JS_FreeValue(ctx, credentials);

        JSValue headers = JS_GetPropertyStr(ctx, argv[1], "headers");
        if (JS_IsObject(headers)) {
            JSPropertyEnum* props = nullptr;
            uint32_t count = 0;
            if (JS_GetOwnPropertyNames(ctx, &props, &count, headers, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                for (uint32_t i = 0; i < count; ++i) {
                    JSValue value = JS_GetProperty(ctx, headers, props[i].atom);
                    const char* name = JS_AtomToCString(ctx, props[i].atom);
                    const char* text = JS_ToCString(ctx, value);
                    if (name && text) {
                        request.headers.set(name, text);
                    }
                    if (name) JS_FreeCString(ctx, name);
                    if (text) JS_FreeCString(ctx, text);
                    JS_FreeValue(ctx, value);
                    JS_FreeAtom(ctx, props[i].atom);
                }
                js_free(ctx, props);
            }
        }
        JS_FreeValue(ctx, headers);
    }

    const std::uint64_t id = engine->host_->start_fetch(request);
    if (id == 0) {
        return reject_with("fetch failed: the request could not be started");
    }
    // The entry owns both functions; settle_fetch (or teardown) frees them.
    engine->pending_fetches_.emplace(id, PendingFetch{functions[0], functions[1]});
    return promise;
}

JSValue QuickJSScriptEngine::make_fetch_payload(const ScriptFetchResponse& response) {
    JSValue payload = JS_NewObject(context_);
    JS_SetPropertyStr(context_, payload, "status", JS_NewInt32(context_, static_cast<int32_t>(response.status)));
    JS_SetPropertyStr(context_, payload, "ok", JS_NewBool(context_, response.ok() ? 1 : 0));
    JS_SetPropertyStr(context_, payload, "url", JS_NewString(context_, response.url.c_str()));
    JS_SetPropertyStr(context_, payload, "body", JS_NewString(context_, response.body.c_str()));

    // Headers as an array of [name, value] pairs, not an object: a response can
    // repeat a field (Set-Cookie), and an object would silently drop all but one.
    JSValue headers = JS_NewArray(context_);
    uint32_t index = 0;
    for (const auto& field : response.headers.fields()) {
        JSValue pair = JS_NewArray(context_);
        JS_SetPropertyUint32(context_, pair, 0, JS_NewString(context_, field.name.c_str()));
        JS_SetPropertyUint32(context_, pair, 1, JS_NewString(context_, field.value.c_str()));
        JS_SetPropertyUint32(context_, headers, index++, pair);
    }
    JS_SetPropertyStr(context_, payload, "headers", headers);
    return payload;
}

bool QuickJSScriptEngine::settle_fetch(const ScriptFetchResponse& response) {
    if (!context_) return false;
    auto it = pending_fetches_.find(response.id);
    if (it == pending_fetches_.end()) {
        // Unknown id: already settled, or cancelled by a navigation that raced
        // the transport. Dropping it is the point — see reject_pending_fetches.
        return false;
    }
    PendingFetch pending = it->second;
    pending_fetches_.erase(it);

    JSValue argument;
    bool resolve = true;
    if (response.failure == ScriptFetchFailure::None) {
        // Per the Fetch standard only a NETWORK error rejects: a 404 or a 500 is
        // a perfectly good response with ok == false, and a page that treats it
        // as a throw is a page that would break on a real server.
        argument = make_fetch_payload(response);
    } else {
        resolve = false;
        // A CORS block deliberately reads like any other failure. Telling the
        // page WHY would hand it the cross-origin information CORS withheld —
        // "blocked by CORS" already reveals that something answered. The engine
        // logs the real reason for the developer; the page learns nothing.
        const char* message = response.failure == ScriptFetchFailure::Timeout ? "fetch timed out"
                              : response.failure == ScriptFetchFailure::BadUrl
                                  ? "fetch failed: unsupported or malformed URL"
                                  : "fetch failed: the network request could not be completed";
        argument = JS_NewError(context_);
        JS_SetPropertyStr(context_, argument, "message", JS_NewString(context_, message));
        // Distinguishable in JS, so a page can retry a timeout without retrying
        // a bad URL (the surfaced half of story 9.1.3).
        JS_SetPropertyStr(
            context_, argument, "name",
            JS_NewString(context_, response.failure == ScriptFetchFailure::Timeout ? "TimeoutError" : "TypeError"));
    }

    {
        // Settling runs the promise's reaction machinery; treat it as a script
        // entry so a nested dispatch cannot steal the microtask checkpoint
        // (9.0.1), and so the drain below is the outermost one.
        ScriptEntryScope entry(script_entry_depth_);
        JSValue ret = JS_Call(context_, resolve ? pending.resolve : pending.reject, JS_UNDEFINED, 1, &argument);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(context_);
            const char* message = JS_ToCString(context_, exc);
            HB_LOG_WARN("[js] fetch settle threw: " << (message ? message : "unknown"));
            if (message) JS_FreeCString(context_, message);
            JS_FreeValue(context_, exc);
        }
        JS_FreeValue(context_, ret);
    }
    JS_FreeValue(context_, argument);
    JS_FreeValue(context_, pending.resolve);
    JS_FreeValue(context_, pending.reject);
    // The continuations the page attached with .then run here.
    drain_microtasks();
    return true;
}

void QuickJSScriptEngine::reject_pending_fetches() {
    if (!context_) {
        pending_fetches_.clear();
        return;
    }
    // Free the callbacks WITHOUT calling them. A navigation has already replaced
    // the document; running a continuation now would execute page A's code
    // against page B's global, which is exactly what 9.0.2 exists to prevent.
    // The promise simply never settles, and its context is freed moments later.
    for (auto& [id, pending] : pending_fetches_) {
        (void)id;
        JS_FreeValue(context_, pending.resolve);
        JS_FreeValue(context_, pending.reject);
    }
    pending_fetches_.clear();
}

// --- Lifecycle -------------------------------------------------------------

QuickJSScriptEngine::QuickJSScriptEngine() {
    runtime_ = JS_NewRuntime();
    if (!runtime_) {
        HB_LOG_ERROR("[script] failed to create QuickJS runtime");
        return;
    }
    register_runtime_classes();
    if (!create_context()) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

QuickJSScriptEngine::~QuickJSScriptEngine() {
    if (context_) {
        release_document_state();
        JS_FreeValue(context_, document_object_);
        document_object_ = JS_UNDEFINED;
        JS_FreeValue(context_, window_object_);
        window_object_ = JS_UNDEFINED;
        JS_FreeValue(context_, node_proto_);
        node_proto_ = JS_UNDEFINED;
        JS_FreeValue(context_, element_proto_);
        element_proto_ = JS_UNDEFINED;
        JS_FreeValue(context_, event_proto_);
        event_proto_ = JS_UNDEFINED;
        JS_FreeContext(context_);
        context_ = nullptr;
    }
    if (runtime_) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

void QuickJSScriptEngine::register_runtime_classes() {
    if (!runtime_) {
        return;
    }
    JSClassDef node_def{};
    node_def.class_name = "Node";
    JS_NewClassID(runtime_, &node_class_id_);
    JS_NewClass(runtime_, node_class_id_, &node_def);

    JSClassDef token_list_def{};
    token_list_def.class_name = "DOMTokenList";
    JS_NewClassID(runtime_, &token_list_class_id_);
    JS_NewClass(runtime_, token_list_class_id_, &token_list_def);

    // dataset is a live string map: property reads/writes intercept through the
    // exotic get/set handlers, which translate keys to data-* attributes. It has
    // no prototype of its own, so a context swap leaves nothing to reinstall.
    static JSClassExoticMethods exotic{};
    exotic.get_property = js_string_map_get;
    exotic.set_property = js_string_map_set;
    JSClassDef string_map_def{};
    string_map_def.class_name = "DOMStringMap";
    string_map_def.exotic = &exotic;
    JS_NewClassID(runtime_, &string_map_class_id_);
    JS_NewClass(runtime_, string_map_class_id_, &string_map_def);
}

bool QuickJSScriptEngine::create_context() {
    if (!runtime_) {
        return false;
    }
    context_ = JS_NewContext(runtime_);
    if (!context_) {
        HB_LOG_ERROR("[script] failed to create QuickJS context");
        return false;
    }
    JS_SetContextOpaque(context_, this);
    // Class IDs are per-runtime, but every context needs its own prototypes.
    install_node_prototype();
    install_token_list_prototype();
    // Install console bindings unconditionally so non-DOM scripts (e.g., extensions)
    // can log without needing to bind a host.
    install_console_bindings();
    // Rebuild whatever surface the current bindings call for: on a context swap
    // the hosts are unchanged, so the next document must not come up bare.
    if (host_) {
        install_document_bindings();
    }
    if (extension_host_) {
        install_extension_bindings();
    }
    return true;
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

void QuickJSScriptEngine::bind_extension_host(IExtensionApiHost* host, std::string_view extension_id) {
    ScriptEngineBase::bind_extension_host(host, extension_id);
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
    std::string error;
    bool threw = false;
    {
        ScriptEntryScope entry(script_entry_depth_);
        JSValue result = JS_Eval(context_, source.data(), source.size(), filename_str.c_str(), JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(context_);
            const char* message = JS_ToCString(context_, exception);
            error = message ? message : "Unknown JS exception";
            if (message) {
                JS_FreeCString(context_, message);
            }
            JS_FreeValue(context_, exception);
            threw = true;
        }
        JS_FreeValue(context_, result);
    }
    // Microtask checkpoint after the script task (7.3.2). Runs on the error path
    // too: promise jobs the script queued before throwing still run.
    drain_microtasks();
    if (threw) {
        record_missing_api_from_error(error);
        return error_result(std::move(error));
    }
    return ok_result();
}

void QuickJSScriptEngine::drain_microtasks() {
    if (!runtime_) return;
    // Nested entry: the JS stack is not empty, so this is not a checkpoint. The
    // outermost entry point drains what the whole re-entrant chain queued
    // (T-DISPATCH-MICROTASK-REENTRANT-1, story 9.0.1).
    if (script_entry_depth_ > 0) return;
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
    release_document_state();
    if (!runtime_ || !context_) {
        return;
    }
    // Give the next document a fresh JS global: a global one page set must not
    // be visible to the next page in the same tab, and a promise continuation
    // captured by a stale global must not survive into it
    // (T-JS-GLOBAL-ISOLATION-1, story 9.0.2). Class IDs live on the runtime and
    // survive; every per-context value is rebuilt by create_context().
    JS_FreeValue(context_, document_object_);
    document_object_ = JS_UNDEFINED;
    JS_FreeValue(context_, window_object_);
    window_object_ = JS_UNDEFINED;
    // Per-context like everything else here: the next context builds its own
    // interface prototypes, and keeping these would leak the old context's
    // objects into it (T-JS-GLOBAL-ISOLATION-1's rule).
    JS_FreeValue(context_, node_proto_);
    node_proto_ = JS_UNDEFINED;
    JS_FreeValue(context_, element_proto_);
    element_proto_ = JS_UNDEFINED;
    JS_FreeValue(context_, event_proto_);
    event_proto_ = JS_UNDEFINED;
    JS_FreeContext(context_);
    context_ = nullptr;
    console_ready_ = false;
    document_ready_ = false;
    extension_ready_ = false;
    failsoft_ready_ = false;
    create_context();
}

void QuickJSScriptEngine::release_document_state() {
    // Pending promise jobs hold a raw JSContext*, so the queue must be empty
    // before the context can be swapped out from under them. It always is:
    // every script entry point drains to exhaustion at its outermost exit
    // (9.0.1), and teardown never runs from inside script. Drain defensively
    // rather than free a context the job queue still points at.
    if (runtime_ && JS_IsJobPending(runtime_)) {
        HB_LOG_WARN("[script] microtask queue not empty at document teardown");
        drain_microtasks();
    }
    // In-flight fetches go first: their continuations are the one kind of
    // callback that can arrive from OUTSIDE the document's own timeline, so a
    // response racing a navigation must find nothing left to settle (9.1.1).
    reject_pending_fetches();
    // Drop event callbacks and timers next: neither may outlive the document,
    // and their callbacks could otherwise still reference a wrapper we are about
    // to free.
    free_listeners();
    free_timers();
    free_animation_frames();
    missing_apis_.clear();            // telemetry is per-document (7.5.2)
    script_location_change_.reset();  // per-document location state (7.7.3)
    script_history_changes_.clear();  // pending History API mutations belong to the old document
    script_history_delta_.reset();    // as does a traversal it requested
    history_state_.clear();           // the next document starts with history.state === null
    history_length_ = 1;              // replaced with the Tab's real count before its scripts run
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

// A DOM interface constructor: `Node`, `Element`, `HTMLElement`.
//
// Not callable. `new HTMLElement()` throws "Illegal constructor" in every
// browser, and a stub that returned an object instead would hand back something
// that looks like an element and is not one.
namespace {
JSValue js_illegal_constructor(JSContext* ctx, JSValueConst /*this_val*/, int /*argc*/, JSValueConst* /*argv*/) {
    return JS_ThrowTypeError(ctx, "Illegal constructor");
}
}  // namespace

void QuickJSScriptEngine::install_dom_interface(const char* name, JSValueConst proto) {
    JSValue global = JS_GetGlobalObject(context_);
    JSValue ctor = JS_NewCFunction2(context_, js_illegal_constructor, name, 0, JS_CFUNC_constructor, 0);
    // `instanceof` walks the object's prototype chain looking for this exact
    // object, which is the whole reason the constructor has to carry the real
    // prototype rather than a fresh one.
    JS_SetConstructor(context_, ctor, proto);
    JS_SetPropertyStr(context_, global, name, ctor);
    JS_FreeValue(context_, global);
}

void QuickJSScriptEngine::install_node_prototype() {
    if (!context_) {
        return;
    }
    // Three prototypes in a real chain, rather than one object carrying
    // everything (T-JS-DOM-INTERFACES-1). Pages do not only call these members,
    // they ASK about them — `x instanceof HTMLElement`, `Element.prototype.foo =
    // …` — and a single flat prototype cannot answer those correctly: a text
    // node would come out as an HTMLElement. The split follows the DOM spec's
    // own division so the answers are right, not merely non-throwing.
    JSValue event_target_proto = JS_NewObject(context_);
    JSValue node_proto = JS_NewObjectProto(context_, event_target_proto);
    JSValue element_proto = JS_NewObjectProto(context_, node_proto);
    JSValue html_proto = JS_NewObjectProto(context_, element_proto);

    // --- EventTarget: the base of the chain ---------------------------------
    // This level was left out when the chain was first built, on the reasoning
    // that it would be "one more object for one more name nothing has asked
    // for". seznam.cz asked for it in the next live sweep, roughly an hour
    // later. Recorded because the reasoning was sound and still wrong: what a
    // real page reaches for is not predictable from what looks load-bearing,
    // which is the entire argument for having the telemetry.
    define_method(event_target_proto, "addEventListener", js_node_add_event_listener, 3);
    define_method(event_target_proto, "removeEventListener", js_node_remove_event_listener, 3);
    define_method(event_target_proto, "dispatchEvent", js_node_dispatch_event, 1);

    // --- Node: structure and text -------------------------------------------
    define_getter(node_proto, "nodeType", js_node_get_node_type);
    define_getter(node_proto, "nodeName", js_node_get_node_name);
    define_accessor(node_proto, "textContent", js_node_get_text_content, js_node_set_text_content);
    define_getter(node_proto, "parentNode", js_node_get_parent_node);
    define_getter(node_proto, "firstChild", js_node_get_first_child);
    define_getter(node_proto, "lastChild", js_node_get_last_child);
    define_getter(node_proto, "nextSibling", js_node_get_next_sibling);
    define_getter(node_proto, "previousSibling", js_node_get_previous_sibling);
    define_getter(node_proto, "childNodes", js_node_get_child_nodes);
    define_method(node_proto, "appendChild", js_node_append_child, 1);
    define_method(node_proto, "insertBefore", js_node_insert_before, 2);
    define_method(node_proto, "removeChild", js_node_remove_child, 1);
    define_method(node_proto, "replaceChild", js_node_replace_child, 2);

    // --- Element: attributes, selectors, element-only traversal --------------
    define_getter(element_proto, "tagName", js_node_get_tag_name);
    define_getter(element_proto, "nextElementSibling", js_node_get_next_element_sibling);
    define_getter(element_proto, "previousElementSibling", js_node_get_previous_element_sibling);
    define_getter(element_proto, "children", js_node_get_children);
    define_accessor(element_proto, "className", js_node_get_class_name, js_node_set_class_name);
    define_accessor(element_proto, "innerHTML", js_node_get_inner_html, js_node_set_inner_html);
    define_getter(element_proto, "classList", js_node_get_class_list);
    define_method(element_proto, "setAttribute", js_element_set_attribute, 2);
    define_method(element_proto, "getAttribute", js_element_get_attribute, 1);
    define_method(element_proto, "removeAttribute", js_element_remove_attribute, 1);
    define_method(element_proto, "querySelector", js_query_selector, 1);
    define_method(element_proto, "querySelectorAll", js_query_selector_all, 1);
    define_method(element_proto, "matches", js_element_matches, 1);
    define_method(element_proto, "closest", js_element_closest, 1);
    define_method(element_proto, "getElementsByClassName", js_get_elements_by_class_name, 1);
    define_method(element_proto, "getElementsByTagName", js_get_elements_by_tag_name, 1);

    // --- HTMLElement: the HTML-only surface ---------------------------------
    // `value`/`checked`/`disabled` really belong to HTMLInputElement and its
    // siblings. Putting them here is a deliberate simplification — one level
    // too low, but far closer than Element and much closer than Node, and it
    // avoids inventing a per-tag interface hierarchy nothing has asked for.
    define_accessor(html_proto, "value", js_node_get_value, js_node_set_value);
    define_accessor(html_proto, "checked", js_node_get_checked, js_node_set_checked);
    define_accessor(html_proto, "disabled", js_node_get_disabled, js_node_set_disabled);
    define_getter(html_proto, "dataset", js_node_get_dataset);
    define_method(html_proto, "focus", js_node_focus, 0);
    define_method(html_proto, "blur", js_node_blur, 0);

    install_dom_interface("EventTarget", event_target_proto);
    install_dom_interface("Node", node_proto);
    install_dom_interface("Element", element_proto);
    install_dom_interface("HTMLElement", html_proto);

    // Kept so wrap_node can pick a prototype per node kind.
    node_proto_ = JS_DupValue(context_, node_proto);
    element_proto_ = JS_DupValue(context_, element_proto);
    JS_FreeValue(context_, event_target_proto);
    JS_FreeValue(context_, node_proto);
    JS_FreeValue(context_, element_proto);

    install_event_constructors();

    // Consumes the reference. HTMLElement.prototype is the DEFAULT for the
    // class, so anything wrapped without an explicit prototype is treated as an
    // element — which is what the overwhelming majority of wrapped nodes are.
    JS_SetClassProto(context_, node_class_id_, html_proto);
}

// `Event` and `CustomEvent`, which pages CONSTRUCT — `new CustomEvent('x', {
// detail })` then `el.dispatchEvent(e)` is the standard way one component
// signals another. Both were ReferenceErrors, so a page doing that died there.
//
// Written in JS rather than as C constructors because that is all they are:
// plain objects carrying a type and a few flags. The engine's own dispatch
// still builds events natively (`make_event`), and the two are tied together by
// sharing this prototype — otherwise a listener testing `e instanceof Event`
// would get different answers depending on who created the event, which is the
// kind of inconsistency that is worse than the absence.
void QuickJSScriptEngine::install_event_constructors() {
    if (!context_) {
        return;
    }
    static constexpr char kEventJs[] = R"JS(
(function (g) {
  function applyInit(ev, type, init) {
    ev.type = String(type);
    ev.bubbles = !!(init && init.bubbles);
    ev.cancelable = !!(init && init.cancelable);
    ev.defaultPrevented = false;
    ev.target = null;
    ev.currentTarget = null;
    ev.eventPhase = 0;
  }
  function Event(type, init) {
    if (!(this instanceof Event)) {
      throw new TypeError("Failed to construct 'Event': Please use the 'new' operator.");
    }
    if (arguments.length === 0) {
      throw new TypeError("Failed to construct 'Event': 1 argument required.");
    }
    applyInit(this, type, init);
  }
  // These are the no-op-safe forms. An event the ENGINE dispatched carries its
  // own native versions as own-properties, which shadow these and are what the
  // dispatcher actually reads; these serve an event the page made itself.
  Event.prototype.preventDefault = function () { if (this.cancelable) this.defaultPrevented = true; };
  Event.prototype.stopPropagation = function () {};
  Event.prototype.stopImmediatePropagation = function () {};

  function CustomEvent(type, init) {
    if (!(this instanceof CustomEvent)) {
      throw new TypeError("Failed to construct 'CustomEvent': Please use the 'new' operator.");
    }
    if (arguments.length === 0) {
      throw new TypeError("Failed to construct 'CustomEvent': 1 argument required.");
    }
    applyInit(this, type, init);
    this.detail = init && 'detail' in init ? init.detail : null;
  }
  CustomEvent.prototype = Object.create(Event.prototype);
  CustomEvent.prototype.constructor = CustomEvent;

  g.Event = Event;
  g.CustomEvent = CustomEvent;
})(globalThis);
)JS";
    JSValue result =
        JS_Eval(context_, kEventJs, std::char_traits<char>::length(kEventJs), "<dom-events>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(context_);
        const char* message = JS_ToCString(context_, exc);
        HB_LOG_WARN("[js] event constructor install failed: " << (message ? message : "unknown"));
        if (message) JS_FreeCString(context_, message);
        JS_FreeValue(context_, exc);
    }
    JS_FreeValue(context_, result);

    // Held so make_event can build engine events on the same prototype.
    JSValue global = JS_GetGlobalObject(context_);
    JSValue event_ctor = JS_GetPropertyStr(context_, global, "Event");
    event_proto_ = JS_GetPropertyStr(context_, event_ctor, "prototype");
    JS_FreeValue(context_, event_ctor);
    JS_FreeValue(context_, global);
}

void QuickJSScriptEngine::install_token_list_prototype() {
    if (!context_ || token_list_class_id_ == 0) {
        return;
    }
    JSValue proto = JS_NewObject(context_);
    define_method(proto, "add", js_token_list_add, 1);
    define_method(proto, "remove", js_token_list_remove, 1);
    define_method(proto, "toggle", js_token_list_toggle, 1);
    define_method(proto, "contains", js_token_list_contains, 1);
    JS_SetClassProto(context_, token_list_class_id_, proto);
}

void QuickJSScriptEngine::define_getter(JSValueConst proto, const char* name, JSCFunction* getter) {
    define_accessor(proto, name, getter, nullptr);
}

void QuickJSScriptEngine::define_getter_magic(JSValueConst target, const char* name, JSCFunctionMagicGetter* getter,
                                              int magic) {
    // Magic getters take (ctx, this, magic), a different call convention from
    // the generic form, so they go through JS_CFUNC_getter_magic with quickjs's
    // standard cast to the union function type — same shape as localStorage's
    // `length`.
    JSAtom atom = JS_NewAtom(context_, name);
    JSValue getter_fn = JS_NewCFunctionMagic(context_, reinterpret_cast<JSCFunctionMagic*>(getter), name, 0,
                                             JS_CFUNC_getter_magic, magic);
    JS_DefinePropertyGetSet(context_, target, atom, getter_fn, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
    JS_FreeAtom(context_, atom);
}

void QuickJSScriptEngine::define_accessor(JSValueConst proto, const char* name, JSCFunction* getter,
                                          JSCFunction* setter) {
    JSAtom atom = JS_NewAtom(context_, name);
    JSValue getter_fn = getter ? JS_NewCFunction(context_, getter, name, 0) : JS_UNDEFINED;
    JSValue setter_fn = setter ? JS_NewCFunction(context_, setter, name, 1) : JS_UNDEFINED;
    JS_DefinePropertyGetSet(context_, proto, atom, getter_fn, setter_fn, JS_PROP_CONFIGURABLE);
    JS_FreeAtom(context_, atom);
}

JSValue QuickJSScriptEngine::make_storage_object(int magic) {
    JSValue obj = JS_NewObject(context_);
    JS_SetPropertyStr(context_, obj, "getItem",
                      JS_NewCFunctionMagic(context_, js_storage_get_item, "getItem", 1, JS_CFUNC_generic_magic, magic));
    JS_SetPropertyStr(context_, obj, "setItem",
                      JS_NewCFunctionMagic(context_, js_storage_set_item, "setItem", 2, JS_CFUNC_generic_magic, magic));
    JS_SetPropertyStr(
        context_, obj, "removeItem",
        JS_NewCFunctionMagic(context_, js_storage_remove_item, "removeItem", 1, JS_CFUNC_generic_magic, magic));
    JS_SetPropertyStr(context_, obj, "clear",
                      JS_NewCFunctionMagic(context_, js_storage_clear, "clear", 0, JS_CFUNC_generic_magic, magic));
    JS_SetPropertyStr(context_, obj, "key",
                      JS_NewCFunctionMagic(context_, js_storage_key, "key", 1, JS_CFUNC_generic_magic, magic));
    // `length` is a magic getter, whose call convention (ctx, this, magic) differs
    // from the generic form, so it is registered through JS_CFUNC_getter_magic
    // with the standard quickjs cast to the union function type.
    JSAtom atom = JS_NewAtom(context_, "length");
    JSValue getter = JS_NewCFunctionMagic(context_, reinterpret_cast<JSCFunctionMagic*>(js_storage_get_length),
                                          "length", 0, JS_CFUNC_getter_magic, magic);
    JS_DefinePropertyGetSet(context_, obj, atom, getter, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
    JS_FreeAtom(context_, atom);
    return obj;
}

void QuickJSScriptEngine::define_method(JSValueConst proto, const char* name, JSCFunction* method, int length) {
    JS_SetPropertyStr(context_, proto, name, JS_NewCFunction(context_, method, name, length));
}

void QuickJSScriptEngine::install_console_bindings() {
    if (console_ready_ || !context_) {
        return;
    }
    JSValue global = JS_GetGlobalObject(context_);

    // `console` used to have exactly one method. A page calling console.warn --
    // which MediaWiki's startup module does -- got "not a function" and died, so
    // the object existing was not the same as the object being usable.
    JSValue console = JS_NewObject(context_);
    const auto add = [&](const char* name, ConsoleLevel level) {
        JS_SetPropertyStr(
            context_, console, name,
            JS_NewCFunctionMagic(context_, js_console_log, name, 1, JS_CFUNC_generic_magic, static_cast<int>(level)));
    };
    add("log", ConsoleLevel::Info);
    add("info", ConsoleLevel::Info);
    add("debug", ConsoleLevel::Info);
    add("dir", ConsoleLevel::Info);
    add("table", ConsoleLevel::Info);
    add("trace", ConsoleLevel::Info);
    add("warn", ConsoleLevel::Warn);
    add("error", ConsoleLevel::Error);
    // Grouping and timing: real methods that log rather than absent ones that
    // throw. Indentation and elapsed times are not worth the state; a page uses
    // these for its own readability, never for control flow.
    add("group", ConsoleLevel::Info);
    add("groupCollapsed", ConsoleLevel::Info);
    add("groupEnd", ConsoleLevel::Info);
    add("time", ConsoleLevel::Info);
    add("timeEnd", ConsoleLevel::Info);
    add("timeLog", ConsoleLevel::Info);
    add("count", ConsoleLevel::Info);
    add("assert", ConsoleLevel::Warn);
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
    // The three document entry points every page assumes exist. Read-only for
    // now: `document.body` is writable per spec, but replacing it wholesale is
    // not something pages actually do, and reading is the gap that broke them.
    define_getter_magic(document, "documentElement", js_document_get_part,
                        static_cast<int>(IScriptHost::DocumentPart::DocumentElement));
    define_getter_magic(document, "body", js_document_get_part, static_cast<int>(IScriptHost::DocumentPart::Body));
    define_getter_magic(document, "head", js_document_get_part, static_cast<int>(IScriptHost::DocumentPart::Head));
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
    define_accessor(document, "cookie", js_document_get_cookie, js_document_set_cookie);
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
    define_getter_magic(location, "protocol", js_location_get_part, static_cast<int>(LocationPart::Protocol));
    define_getter_magic(location, "host", js_location_get_part, static_cast<int>(LocationPart::Host));
    define_getter_magic(location, "hostname", js_location_get_part, static_cast<int>(LocationPart::Hostname));
    define_getter_magic(location, "port", js_location_get_part, static_cast<int>(LocationPart::Port));
    define_getter_magic(location, "pathname", js_location_get_part, static_cast<int>(LocationPart::Pathname));
    define_getter_magic(location, "search", js_location_get_part, static_cast<int>(LocationPart::Search));
    define_getter_magic(location, "origin", js_location_get_part, static_cast<int>(LocationPart::Origin));
    // `String(location)` and string concatenation both yield the full URL.
    JS_SetPropertyStr(context_, location, "toString", JS_NewCFunction(context_, js_location_get_href, "toString", 0));

    // `window` IS the global object (T-JS-WINDOW-IS-GLOBAL-1). This used to be a
    // separate JS_NewObject onto which a hand-picked subset of globals was
    // mirrored, which meant everything NOT on that list was missing from
    // `window` — `window.console`, `window.document`, `window.navigator`,
    // `window.fetch`, every fail-soft stub — and the list had to grow by hand
    // every time a global was added. It fell behind, and MediaWiki's startup
    // module died on `window.console.warn`.
    //
    // A browser has exactly one object here: `window === globalThis`. Making the
    // self-reference real means every global is reachable both ways for free,
    // now and for anything added later, and `window.foo = 1; foo` aliases the
    // way script expects. The global->window->global cycle is what browsers have
    // too; quickjs's GC collects cycles.
    JS_SetPropertyStr(context_, global, "window", JS_DupValue(context_, global));
    // `self` is the same object again. It exists because worker scopes have no
    // `window`, so library code that must run in both writes `self` — which is
    // most bundled/UMD script, and is why this was a ReferenceError on
    // seznam.cz on every single load in the live sweep. Free to provide now
    // that the global self-reference is real; before that it would have been a
    // third object to keep in sync.
    JS_SetPropertyStr(context_, global, "self", JS_DupValue(context_, global));
    window_object_ = JS_DupValue(context_, global);

    // window is an EventTarget (hashchange fires here). On the global, so both
    // `window.addEventListener` and a bare `addEventListener` — which was
    // previously a ReferenceError — reach the window target.
    JS_SetPropertyStr(context_, global, "addEventListener",
                      JS_NewCFunction(context_, js_node_add_event_listener, "addEventListener", 3));
    JS_SetPropertyStr(context_, global, "removeEventListener",
                      JS_NewCFunction(context_, js_node_remove_event_listener, "removeEventListener", 3));
    JS_SetPropertyStr(context_, global, "dispatchEvent",
                      JS_NewCFunction(context_, js_node_dispatch_event, "dispatchEvent", 1));

    // localStorage (8.2.2) + sessionStorage (8.2.3). One object each per
    // document; the methods route through the host to the right StorageArea
    // (local = shared/persisted, session = per-tab), so no per-object opaque
    // state is needed. Set unconditionally so they win over the 7.5.2 fail-soft
    // stubs (whose typeof guards then skip).
    JSValue local_storage = make_storage_object(/*magic=*/0);
    JSValue session_storage = make_storage_object(/*magic=*/1);

    // history (9.6.1). pushState/replaceState share one callback via magic; the
    // traversal trio does the same with its delta.
    JSValue history = JS_NewObject(context_);
    JS_SetPropertyStr(context_, history, "pushState",
                      JS_NewCFunctionMagic(context_, js_history_push_state, "pushState", 3, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(
        context_, history, "replaceState",
        JS_NewCFunctionMagic(context_, js_history_push_state, "replaceState", 3, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(context_, history, "back",
                      JS_NewCFunctionMagic(context_, js_history_go, "back", 0, JS_CFUNC_generic_magic, -1));
    JS_SetPropertyStr(context_, history, "forward",
                      JS_NewCFunctionMagic(context_, js_history_go, "forward", 0, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(context_, history, "go",
                      JS_NewCFunctionMagic(context_, js_history_go, "go", 1, JS_CFUNC_generic_magic, 0));
    define_getter_magic(history, "state", js_history_get_state, 0);
    define_getter_magic(history, "length", js_history_get_length, 0);
    JS_SetPropertyStr(context_, global, "history", history);  // transfers the ref

    JS_SetPropertyStr(context_, global, "location", location);               // transfers the ref
    JS_SetPropertyStr(context_, global, "localStorage", local_storage);      // transfers the ref
    JS_SetPropertyStr(context_, global, "sessionStorage", session_storage);  // transfers the ref
    JS_SetPropertyStr(context_, global, "setTimeout", JS_NewCFunction(context_, js_set_timeout, "setTimeout", 2));
    JS_SetPropertyStr(context_, global, "setInterval", JS_NewCFunction(context_, js_set_interval, "setInterval", 2));
    JS_SetPropertyStr(context_, global, "clearTimeout", JS_NewCFunction(context_, js_clear_timer, "clearTimeout", 1));
    JS_SetPropertyStr(context_, global, "clearInterval", JS_NewCFunction(context_, js_clear_timer, "clearInterval", 1));
    JS_SetPropertyStr(context_, global, "requestAnimationFrame",
                      JS_NewCFunction(context_, js_request_animation_frame, "requestAnimationFrame", 1));
    JS_SetPropertyStr(context_, global, "cancelAnimationFrame",
                      JS_NewCFunction(context_, js_cancel_animation_frame, "cancelAnimationFrame", 1));
    JS_SetPropertyStr(context_, global, "__hb_nativeFetch",
                      JS_NewCFunction(context_, js_native_fetch, "__hb_nativeFetch", 2));
    JS_FreeValue(context_, global);

    install_failsoft_stubs();  // fail-soft stubs for unimplemented APIs (7.5.2)
    install_fetch_prelude();   // Response/Headers ergonomics over the binding (9.1.1)
}

// The JS half of fetch (9.1.1). The native binding is a data hand-off — a URL
// and options in, a plain payload out — and this shapes that payload into the
// Response/Headers surface a page expects. Keeping it here rather than in C++
// keeps the binding small and the object ergonomics readable.
//
// Installed AFTER the fail-soft stubs so it wins: the stubs only define names
// that are still undefined, and fetch is no longer among them.
void QuickJSScriptEngine::install_fetch_prelude() {
    if (!context_) {
        return;
    }
    static constexpr char kPrelude[] = R"JS(
(function (g) {
  'use strict';
  function Headers(pairs) {
    // Field names are case-insensitive, so store lowercased and look up the
    // same way. Repeated fields (Set-Cookie) join with ", " as the spec says.
    var map = {};
    var order = [];
    (pairs || []).forEach(function (pair) {
      var name = String(pair[0]).toLowerCase();
      var value = String(pair[1]);
      if (Object.prototype.hasOwnProperty.call(map, name)) {
        map[name] = map[name] + ', ' + value;
      } else {
        map[name] = value;
        order.push(name);
      }
    });
    this.get = function (name) {
      var key = String(name).toLowerCase();
      return Object.prototype.hasOwnProperty.call(map, key) ? map[key] : null;
    };
    this.has = function (name) {
      return Object.prototype.hasOwnProperty.call(map, String(name).toLowerCase());
    };
    this.forEach = function (fn, thisArg) {
      order.forEach(function (name) { fn.call(thisArg, map[name], name, this); }, this);
    };
    this.keys = function () { return order.slice(); };
  }

  function Response(raw) {
    this.status = raw.status;
    this.ok = raw.ok;
    this.url = raw.url;
    this.headers = new Headers(raw.headers);
    this.redirected = false;
    var body = raw.body;
    var used = false;
    function takeBody() {
      // The spec makes a body single-use; enforcing it here catches the common
      // "read it twice and get an empty string" bug at the point of the mistake.
      if (used) { throw new TypeError('body has already been read'); }
      used = true;
      return body;
    }
    Object.defineProperty(this, 'bodyUsed', { get: function () { return used; } });
    this.text = function () {
      try { return Promise.resolve(takeBody()); } catch (e) { return Promise.reject(e); }
    };
    this.json = function () {
      try { return Promise.resolve(JSON.parse(takeBody())); } catch (e) { return Promise.reject(e); }
    };
  }

  // Accepts a Headers instance, an array of pairs, or a plain object, and hands
  // the binding the one shape it reads.
  function normalizeHeaders(input) {
    if (!input) return undefined;
    var out = {};
    if (typeof input.forEach === 'function' && typeof input.get === 'function') {
      input.forEach(function (value, name) { out[name] = value; });
      return out;
    }
    if (Array.isArray(input)) {
      input.forEach(function (pair) { out[String(pair[0])] = String(pair[1]); });
      return out;
    }
    Object.keys(input).forEach(function (name) { out[name] = String(input[name]); });
    return out;
  }

  g.Headers = Headers;
  g.Response = Response;
  g.fetch = function (input, init) {
    var options = init || {};
    var request = {
      method: options.method,
      body: options.body,
      headers: normalizeHeaders(options.headers)
    };
    return g.__hb_nativeFetch(String(input), request).then(function (raw) {
      return new Response(raw);
    });
  };
})(globalThis);
)JS";
    JSValue result =
        JS_Eval(context_, kPrelude, std::char_traits<char>::length(kPrelude), "<fetch>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(context_);
        const char* message = JS_ToCString(context_, exc);
        HB_LOG_WARN("[js] fetch prelude install failed: " << (message ? message : "unknown"));
        if (message) JS_FreeCString(context_, message);
        JS_FreeValue(context_, exc);
    }
    JS_FreeValue(context_, result);
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

// --- History API MVP (9.6.1) ------------------------------------------------
//
// pushState/replaceState are a *session history* operation, not a navigation:
// the document must not be torn down, so the binding only records what the page
// asked for and updates `location` in place. The Tab drains the request, because
// it owns the history stack and the URL bar.
//
// `title` (argv[1]) is accepted and ignored, which is what browsers do — it was
// never implemented by any of them.
JSValue QuickJSScriptEngine::js_history_push_state(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                                                   JSValueConst* argv, int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine) {
        return JS_UNDEFINED;
    }

    // Serialize the state now rather than holding a JSValue: it has to survive
    // in the Tab's history stack, which outlives this document's JS context.
    // JSON is the MVP's stated limit (structured clone is M12's).
    std::string serialized;
    if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        JSValue json = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
        if (JS_IsException(json)) {
            // A non-serializable state is a DataCloneError in the spec. Report it
            // rather than storing something the page did not ask for.
            JS_FreeValue(ctx, json);
            return JS_ThrowTypeError(ctx, "history state could not be serialized");
        }
        if (const char* text = JS_ToCString(ctx, json)) {
            serialized = text;
            JS_FreeCString(ctx, text);
        }
        JS_FreeValue(ctx, json);
    }

    // An omitted or empty url means "the current one", per spec.
    std::string url = engine->location_url_;
    if (argc >= 3 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        if (const char* text = JS_ToCString(ctx, argv[2])) {
            const std::string requested(text);
            JS_FreeCString(ctx, text);
            if (!requested.empty()) {
                // Resolved against `location_url_`, not through the host resolver
                // that fetch uses. Two reasons: pushState updates location_url_
                // synchronously, so a second relative push inside the same script
                // run chains off the first (the host's base only moves once the
                // Tab drains); and it keeps the API working with no resolver
                // wired, which a location operation should not depend on.
                url = Core::resolve_url(engine->location_url_, requested);
                const auto document_origin = Core::Origin::parse(engine->location_url_);
                const auto target_origin = Core::Origin::parse(url);
                if (!document_origin || !target_origin || *document_origin != *target_origin) {
                    JSValue error = JS_NewError(ctx);
                    JS_SetPropertyStr(ctx, error, "name", JS_NewString(ctx, "SecurityError"));
                    JS_SetPropertyStr(
                        ctx, error, "message",
                        JS_NewString(ctx,
                                     "Failed to execute 'pushState' on 'History': the URL has a different origin."));
                    return JS_Throw(ctx, error);
                }
            }
        }
    }

    const bool replace = magic != 0;
    engine->history_state_ = serialized;
    // location reflects the new URL immediately — a page that pushes and then
    // reads location.href must see the pushed address, and no hashchange fires
    // for a pushState even when the fragment differs.
    engine->location_url_ = url;
    engine->script_history_changes_.push_back(HistoryChange{url, serialized, replace});
    return JS_UNDEFINED;
}

JSValue QuickJSScriptEngine::js_history_get_state(JSContext* ctx, JSValueConst /*this_val*/, int /*magic*/) {
    auto* engine = engine_from_context(ctx);
    if (!engine || engine->history_state_.empty()) {
        return JS_NULL;  // no state is null, which is distinct from the string "null"
    }
    return JS_ParseJSON(ctx, engine->history_state_.c_str(), engine->history_state_.size(), "<history.state>");
}

JSValue QuickJSScriptEngine::js_history_get_length(JSContext* ctx, JSValueConst /*this_val*/, int /*magic*/) {
    auto* engine = engine_from_context(ctx);
    return JS_NewInt64(ctx, engine ? static_cast<int64_t>(engine->history_length_) : 1);
}

// back()/forward()/go(n). magic is the fixed delta, or 0 for go(n) which reads
// its own. Only the request is recorded: traversal needs the Tab, which owns the
// stack and the graphics context a re-render requires.
JSValue QuickJSScriptEngine::js_history_go(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv,
                                           int magic) {
    auto* engine = engine_from_context(ctx);
    if (!engine) {
        return JS_UNDEFINED;
    }
    int delta = magic;
    if (magic == 0) {
        int32_t requested = 0;
        if (argc >= 1 && JS_ToInt32(ctx, &requested, argv[0]) == 0) {
            delta = requested;
        }
        // go(0) reloads in a browser; the MVP treats it as a no-op rather than
        // pretending to implement a reload from here.
        if (delta == 0) {
            return JS_UNDEFINED;
        }
    }
    engine->script_history_delta_ = delta;
    return JS_UNDEFINED;
}

std::optional<IScriptEngine::HistoryChange> QuickJSScriptEngine::consume_history_change() {
    if (script_history_changes_.empty()) return std::nullopt;
    HistoryChange out = std::move(script_history_changes_.front());
    script_history_changes_.pop_front();
    return out;
}

std::optional<int> QuickJSScriptEngine::consume_history_delta() {
    std::optional<int> out = script_history_delta_;
    script_history_delta_.reset();
    return out;
}

void QuickJSScriptEngine::set_history_length(size_t length) {
    history_length_ = length == 0 ? 1 : length;
}

bool QuickJSScriptEngine::apply_popstate(std::string_view url, std::string_view state) {
    if (!context_) {
        return false;
    }
    location_url_ = std::string(url);
    history_state_ = std::string(state);
    // A traversal the app drove: it must NOT report back as a script-initiated
    // change, or the Tab would re-push the entry it just moved to.
    script_history_changes_.clear();
    script_location_change_.reset();

    // Same shape as the hashchange dispatch above: a real event object on the
    // window target, bracketed by a ScriptEntryScope, with a microtask
    // checkpoint after the listeners (7.3.2).
    JSValue event = make_event("popstate", window_target());
    JSValue state_value = history_state_.empty()
                              ? JS_NULL
                              : JS_ParseJSON(context_, history_state_.c_str(), history_state_.size(), "<popstate>");
    if (JS_IsException(state_value)) {
        JS_FreeValue(context_, state_value);
        state_value = JS_NULL;
    }
    JS_SetPropertyStr(context_, event, "state", state_value);
    {
        ScriptEntryScope entry(script_entry_depth_);
        dispatch_event(window_target(), "popstate", event);
    }
    JS_FreeValue(context_, event);
    drain_microtasks();
    // Whether a listener mutated the DOM is the controller's to report — it owns
    // the host's mutation epoch, exactly as it does for hashchange. Returning
    // true here means only "the event was dispatched".
    return true;
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
    {
        ScriptEntryScope entry(script_entry_depth_);
        dispatch_event(window_target(), "hashchange", event);
    }
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

        {
            ScriptEntryScope entry(script_entry_depth_);
            JSValue ret =
                JS_Call(context_, callback, JS_UNDEFINED, static_cast<int>(call_args.size()), call_args.data());
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(context_);
                const char* message = JS_ToCString(context_, exc);
                HB_LOG_WARN("[js] timer callback threw: " << (message ? message : "unknown"));
                if (message) JS_FreeCString(context_, message);
                JS_FreeValue(context_, exc);
            }
            JS_FreeValue(context_, ret);
        }
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
        {
            ScriptEntryScope entry(script_entry_depth_);
            JSValue ret = JS_Call(context_, cb.callback, JS_UNDEFINED, 1, &arg);
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(context_);
                const char* message = JS_ToCString(context_, exc);
                HB_LOG_WARN("[js] requestAnimationFrame callback threw: " << (message ? message : "unknown"));
                if (message) JS_FreeCString(context_, message);
                JS_FreeValue(context_, exc);
            }
            JS_FreeValue(context_, ret);
        }
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

// Story 9.5.2 follow-up. The fail-soft stub list can only report gaps somebody
// predicted; a live sweep showed the loudest real signal is the opposite —
// scripts dying on `ReferenceError: X is not defined`, naming a global nobody
// had thought to stub. Wikipedia's bundle died on `Element`, which was not on
// the 14-name list and would never have appeared in the telemetry.
//
// Recorded with a `(ReferenceError)` suffix rather than as a plain name,
// because the two are not the same finding and a triage must not merge them: a
// stub hit means the page carried on without the feature, while this means the
// script DIED at that point and everything after it never ran.
void QuickJSScriptEngine::record_missing_api_from_error(std::string_view error) {
    constexpr std::string_view kPrefix = "ReferenceError: ";
    constexpr std::string_view kSuffix = " is not defined";
    if (!error.starts_with(kPrefix) || !error.ends_with(kSuffix)) {
        return;
    }
    std::string_view name = error.substr(kPrefix.size(), error.size() - kPrefix.size() - kSuffix.size());
    // Minified bundles throw on their own mangled locals (`aa is not defined`),
    // which say nothing about this engine. A real global is not one or two
    // characters long, so that cheap filter removes the bulk of the noise
    // without needing to know which names are real.
    if (name.size() < 3 || name.find(' ') != std::string_view::npos) {
        return;
    }
    record_missing_api(std::string(name) + " (ReferenceError)");
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
  // NOTE: no fetch stub here any more (story 9.1.1). It used to be
  //   g.fetch = function () { return new Promise(function () {}); };
  // which never settled, so a page using fetch did not fail — it froze its own
  // logic forever, silently. Real fetch is installed by install_window_bindings;
  // if that did not happen there is no host, and the binding rejects rather than
  // leaving a promise hanging.
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
  // --- Story T-JS-MISSING-API-COVERAGE-1 -----------------------------------
  // Before this the reporting surface was four names, two of which cover
  // features implemented back in 8.2.2/8.2.3 and so never fire. Two observable
  // APIs cannot carry the roadmap's plan of deriving M12's scope from this
  // telemetry, and they quietly biased 9.1.2: XHR's "only if telemetry reports
  // it" trigger could only ever fire for one of the two things visible.
  //
  // Each stub below is `typeof`-guarded (a real implementation later wins), is
  // a no-op that CANNOT throw, and reports exactly once per document. None of
  // them fakes a plausible value a page could branch on incorrectly — an empty
  // answer is honest, a wrong answer is a bug the page then blames on itself.

  // Constructor-shaped observers. The page does `new X(cb)` and then calls
  // methods on the instance, so the prototype must exist or the report is
  // immediately followed by a TypeError, which is the failure this avoids.
  function observerStub(name, extras) {
    var Ctor = function () { report(name); };
    Ctor.prototype.observe = function () {};
    Ctor.prototype.unobserve = function () {};
    Ctor.prototype.disconnect = function () {};
    if (extras) { Ctor.prototype.takeRecords = function () { return []; }; }
    return Ctor;
  }
  if (typeof g.IntersectionObserver === 'undefined') {
    g.IntersectionObserver = observerStub('IntersectionObserver', true);
  }
  if (typeof g.MutationObserver === 'undefined') {
    g.MutationObserver = observerStub('MutationObserver', true);
  }
  if (typeof g.ResizeObserver === 'undefined') {
    g.ResizeObserver = observerStub('ResizeObserver', false);
  }
  if (typeof g.PerformanceObserver === 'undefined') {
    g.PerformanceObserver = observerStub('PerformanceObserver', true);
  }

  if (typeof g.customElements === 'undefined') {
    g.customElements = {
      // A defined element simply never upgrades. The page's markup still
      // renders as unknown elements, which is what it does before upgrade
      // anyway, so this degrades rather than breaks.
      define: function () { report('customElements'); },
      get: function () { report('customElements'); return undefined; },
      upgrade: function () { report('customElements'); },
      // Never resolves: whenDefined promises an upgrade that is not coming, and
      // resolving it would run the page's post-upgrade code against an element
      // that was never upgraded. A pending promise stalls that one continuation;
      // a false resolve corrupts everything after it.
      whenDefined: function () { report('customElements'); return new Promise(function () {}); }
    };
  }

  if (typeof g.WebSocket === 'undefined') {
    g.WebSocket = function () {
      report('WebSocket');
      this.readyState = 3;  // CLOSED — never pretend a connection is open
      this.bufferedAmount = 0;
    };
    g.WebSocket.prototype.send = function () {};
    g.WebSocket.prototype.close = function () {};
    g.WebSocket.prototype.addEventListener = function () {};
    g.WebSocket.prototype.removeEventListener = function () {};
    g.WebSocket.CONNECTING = 0; g.WebSocket.OPEN = 1; g.WebSocket.CLOSING = 2; g.WebSocket.CLOSED = 3;
  }

  if (typeof g.requestIdleCallback === 'undefined') {
    // Deliberately runs the callback (on a timer) rather than dropping it:
    // pages defer real initialization into idle callbacks, and never calling
    // them leaves the page half-built with no error to explain why.
    g.requestIdleCallback = function (cb) {
      report('requestIdleCallback');
      return g.setTimeout(function () {
        cb({ didTimeout: false, timeRemaining: function () { return 0; } });
      }, 1);
    };
    g.cancelIdleCallback = function (id) { g.clearTimeout(id); };
  }

  if (typeof g.getComputedStyle === 'undefined') {
    // Reports empty for every property. The engine HAS computed styles, but
    // exposing them is a real feature (T-JS-GET-COMPUTED-STYLE-1), not a stub —
    // and a stub returning made-up lengths would be worse than an empty one,
    // because layout-reading code would act on the numbers.
    g.getComputedStyle = function () {
      report('getComputedStyle');
      return { getPropertyValue: function () { return ''; }, getPropertyPriority: function () { return ''; },
               length: 0, item: function () { return ''; } };
    };
  }

  if (typeof g.navigator === 'undefined') {
    // `navigator` did not exist at all, so `navigator.userAgent` — which a very
    // large share of real pages read — was a ReferenceError that killed the
    // whole script. userAgent is deliberately EMPTY rather than a plausible
    // string: this engine has a considered per-origin identity policy (M8), and
    // a JS surface that answers with something different from what the network
    // layer sends would be lying in two directions at once. Filling it in from
    // the identity store is T-JS-NAVIGATOR-IDENTITY-1.
    g.navigator = {
      get userAgent() { report('navigator.userAgent'); return ''; },
      language: 'en-US',
      languages: ['en-US'],
      onLine: true,
      cookieEnabled: true,
      platform: '',
      // Strings, not undefined. A live sweep caught Google Tag Manager doing
      // `.indexOf` on one of these and dying on `undefined` — the stub existing
      // is not enough if the shape is wrong.
      appVersion: '',
      appName: 'Netscape',
      product: 'Gecko',
      vendor: '',
      doNotTrack: null,
      hardwareConcurrency: 1,
      maxTouchPoints: 0
    };
  }

  if (typeof g.alert === 'undefined') {
    // No dialog surface exists. Reporting beats a ReferenceError mid-script.
    g.alert = function () { report('alert'); };
    g.confirm = function () { report('confirm'); return false; };
    g.prompt = function () { report('prompt'); return null; };
  }

  if (typeof g.structuredClone === 'undefined') {
    g.structuredClone = function (value) {
      report('structuredClone');
      // A JSON round-trip is a correct deep copy for JSON-shaped data, which is
      // what pages clone in practice, and returns the input unchanged when it
      // is not — never throws, never aliases silently for the common case.
      try { return JSON.parse(JSON.stringify(value)); } catch (e) { return value; }
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
    JS_SetPropertyStr(context_, global, "__hb_nativeSetFilterRules",
                      JS_NewCFunction(context_, js_native_set_filter_rules, "__hb_nativeSetFilterRules", 1));
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
    // A text node is not an HTMLElement, so it must not inherit from one
    // (T-JS-DOM-INTERFACES-1). Getting this wrong would make `instanceof`
    // answer confidently and wrongly, which is worse than the ReferenceError
    // these interfaces replaced: a page branching on it takes the wrong path
    // silently.
    JSValue obj;
    if (host_ && host_->node_kind(node) != NodeKind::Element && !JS_IsUndefined(node_proto_)) {
        obj = JS_NewObjectProtoClass(context_, node_proto_, node_class_id_);
    } else {
        obj = JS_NewObjectClass(context_, node_class_id_);
    }
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
