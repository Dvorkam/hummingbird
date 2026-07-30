#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/ScriptFetch.h"

namespace Hummingbird::DOM {
class Element;
class Node;
}  // namespace Hummingbird::DOM

namespace Hummingbird {

// How a DOM node presents to the JS binding layer (mirrors the subset of the
// DOM `nodeType` constants Milestone 7 needs).
enum class NodeKind {
    Element,
    Text,
    Other,
};

// Boundary the JS engine (platform adapter) calls into to read and mutate the
// live document. The adapter only ever holds opaque DOM::Node*/Element* handles;
// every tree operation and every ownership decision lives behind this port.
class IScriptHost {
public:
    virtual ~IScriptHost() = default;

    // The three standard document entry points (T-DOM-DOCUMENT-BODY-1). One
    // enum-keyed accessor rather than three near-identical port methods, the
    // shape StorageKind already uses — the alternative grows this interface by
    // one virtual per property forever.
    enum class DocumentPart {
        DocumentElement,  // the root <html> element
        Body,             // <body> (or <frameset>, per the HTML spec)
        Head,             // <head>
    };

    // --- Lookup + content (M6) ---
    virtual DOM::Element* get_element_by_id(std::string_view id) = 0;
    // Returns nullptr when the document has no such element. That is a real
    // answer, not a failure: this engine's parser does not synthesize the
    // html/head/body skeleton a browser's tree construction would, so a
    // document written without those tags genuinely has no body element
    // (T-HTML-TREE-SKELETON-1). Callers must handle null rather than assume.
    virtual DOM::Element* document_part(DocumentPart part) = 0;
    virtual std::string get_text_content(const DOM::Node* node) = 0;
    virtual void set_text_content(DOM::Node* node, std::string_view text) = 0;
    virtual void set_attribute(DOM::Node* node, std::string_view name, std::string_view value) = 0;

    // --- Attributes / classList / dataset (7.1.2) ---
    virtual bool has_attribute(DOM::Node* node, std::string_view name) = 0;
    // Attribute value, or "" when absent (pair with has_attribute for null checks).
    virtual std::string get_attribute(DOM::Node* node, std::string_view name) = 0;
    virtual void remove_attribute(DOM::Node* node, std::string_view name) = 0;

    virtual bool class_list_contains(DOM::Node* node, std::string_view token) = 0;
    virtual void class_list_add(DOM::Node* node, std::string_view token) = 0;
    virtual void class_list_remove(DOM::Node* node, std::string_view token) = 0;
    // Adds the token when absent / removes it when present; returns final membership.
    virtual bool class_list_toggle(DOM::Node* node, std::string_view token) = 0;

    // dataset uses the DOM camelCase<->data-* mapping ("userId" <-> "data-user-id").
    // Returns true (and fills out) when the mapped attribute is present.
    virtual bool get_dataset(DOM::Node* node, std::string_view key, std::string& out) = 0;
    virtual void set_dataset(DOM::Node* node, std::string_view key, std::string_view value) = 0;

    // --- Form control surface (7.1.5) ---
    // Reflect the form-control state JS reads/writes. `value` is the control's
    // current value; `checked`/`disabled` are boolean attribute state. Focus is
    // reflected as the :focus pseudo-state (full text-edit focus wiring + the
    // checkbox control land with the event system in 7.2.4).
    virtual std::string get_value(DOM::Node* node) = 0;
    virtual void set_value(DOM::Node* node, std::string_view value) = 0;
    virtual bool get_checked(DOM::Node* node) = 0;
    virtual void set_checked(DOM::Node* node, bool checked) = 0;
    virtual bool get_disabled(DOM::Node* node) = 0;
    virtual void set_disabled(DOM::Node* node, bool disabled) = 0;
    virtual void set_focused(DOM::Node* node, bool focused) = 0;

    // --- document.cookie (8.1.5) ---
    // The script-visible cookie string for the current document: same-origin,
    // non-HttpOnly cookies only. Empty when there are none, or when no cookie
    // jar is wired up.
    virtual std::string get_document_cookie() = 0;
    // Applies one `Set-Cookie`-shaped string from script. Parsed by the same jar
    // code as a server header, so attribute handling cannot drift between the
    // two paths; a cookie script may not set is silently ignored, as in a real
    // browser.
    virtual void set_document_cookie(std::string_view value) = 0;

    // --- window.localStorage / sessionStorage (8.2.2 / 8.2.3) ---
    // The current document's origin store. `kind` selects which of the two Web
    // Storage areas: Local is shared per profile and persisted (8.2.2); Session
    // is per-tab and never persisted (8.2.3). When no store is available (opaque
    // origin, or no store wired up) reads are empty and writes are dropped,
    // mirroring how document.cookie behaves with no jar.
    enum class StorageKind { Local, Session };
    enum class StorageWriteResult {
        Ok,             // stored (or dropped because no store exists)
        QuotaExceeded,  // refused; the binding raises QuotaExceededError
    };
    virtual std::optional<std::string> storage_get_item(StorageKind kind, std::string_view key) = 0;
    virtual StorageWriteResult storage_set_item(StorageKind kind, std::string_view key, std::string_view value) = 0;
    virtual void storage_remove_item(StorageKind kind, std::string_view key) = 0;
    virtual void storage_clear(StorageKind kind) = 0;
    virtual size_t storage_length(StorageKind kind) = 0;
    virtual std::optional<std::string> storage_key(StorageKind kind, size_t index) = 0;

    // --- innerHTML (7.1.4) ---
    // Replaces node's children with the fragment parsed from `html` (reuses the
    // document HTML parser in a recovery-oriented fragment mode).
    virtual void set_inner_html(DOM::Node* node, std::string_view html) = 0;
    // Serializes node's children back to HTML.
    virtual std::string get_inner_html(DOM::Node* node) = 0;

    // --- Selector queries (7.1.3) ---
    // `scope` is the element to search within; nullptr means the document root.
    // Queries reuse the style engine's selector parser + matcher, so the
    // supported subset is identical to CSS. Results are document-order snapshots.
    virtual DOM::Node* query_selector(DOM::Node* scope, std::string_view selector) = 0;
    virtual std::vector<DOM::Node*> query_selector_all(DOM::Node* scope, std::string_view selector) = 0;
    virtual bool matches(DOM::Node* node, std::string_view selector) = 0;
    virtual DOM::Node* closest(DOM::Node* node, std::string_view selector) = 0;
    // Legacy live-collection accessors (returned here as static snapshots).
    virtual std::vector<DOM::Node*> get_elements_by_class_name(DOM::Node* scope, std::string_view names) = 0;
    virtual std::vector<DOM::Node*> get_elements_by_tag_name(DOM::Node* scope, std::string_view tag) = 0;

    // --- Node factories (7.1.1) ---
    // Created nodes are detached (no parent) and owned by the host until they
    // are inserted into the tree.
    virtual DOM::Element* create_element(std::string_view tag_name) = 0;
    virtual DOM::Node* create_text_node(std::string_view data) = 0;

    // --- Tree mutation (7.1.1) ---
    // Each returns the affected child on success (the inserted node, or the
    // removed/replaced node) or nullptr when the operation is rejected
    // (missing arena, hierarchy violation, wrong parent, etc.).
    virtual DOM::Node* append_child(DOM::Node* parent, DOM::Node* child) = 0;
    virtual DOM::Node* insert_before(DOM::Node* parent, DOM::Node* child, DOM::Node* reference) = 0;
    virtual DOM::Node* remove_child(DOM::Node* parent, DOM::Node* child) = 0;
    virtual DOM::Node* replace_child(DOM::Node* parent, DOM::Node* new_child, DOM::Node* old_child) = 0;

    // --- Traversal accessors (7.1.1) ---
    virtual DOM::Node* parent_node(DOM::Node* node) = 0;
    virtual DOM::Node* first_child(DOM::Node* node) = 0;
    virtual DOM::Node* last_child(DOM::Node* node) = 0;
    virtual DOM::Node* next_sibling(DOM::Node* node) = 0;
    virtual DOM::Node* previous_sibling(DOM::Node* node) = 0;
    virtual DOM::Node* next_element_sibling(DOM::Node* node) = 0;
    virtual DOM::Node* previous_element_sibling(DOM::Node* node) = 0;
    virtual std::vector<DOM::Node*> child_nodes(DOM::Node* node) = 0;
    virtual std::vector<DOM::Node*> child_elements(DOM::Node* node) = 0;

    // --- Node metadata for wrappers (7.1.1) ---
    virtual NodeKind node_kind(const DOM::Node* node) = 0;
    // Uppercase tag name for elements ("DIV"), "#text" for text nodes, "" otherwise.
    virtual std::string node_name(const DOM::Node* node) = 0;

    // --- fetch (9.1.1) ---
    // Starts a request and returns its id, or 0 when the host cannot take it
    // (no network wired up). The response arrives later via
    // IScriptEngine::settle_fetch on the main thread — never from the callback's
    // own thread, and never after the document has been torn down.
    //
    // Default: no network. Most unit tests bind a host that has none, and a page
    // that calls fetch there should get a clean rejection rather than a hang.
    virtual std::uint64_t start_fetch(const ScriptFetchRequest& /*request*/) { return 0; }
    // Resolves `relative` against the document's base URL. Lives here because
    // only the engine knows the document's URL; the adapter holds no such state.
    virtual std::string resolve_url(std::string_view relative) { return std::string(relative); }
};

}  // namespace Hummingbird
