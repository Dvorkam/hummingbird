#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/net/StorageArea.h"
#include "core/platform_api/IScriptHost.h"
#include "core/platform_api/ScriptFetch.h"

namespace Hummingbird::DOM {
class Element;
class Node;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Engine {

class DocumentScriptHost final : public IScriptHost {
public:
    DocumentScriptHost();
    ~DocumentScriptHost() override;

    void reset(DOM::Node* root, Core::ArenaAllocator* arena);
    void clear();
    bool consume_mutations();

    // Brackets one script dispatch so nested dispatches (a host callback that
    // re-enters the controller — a timer firing an event, JS focus() firing
    // focus) share the outer dispatch's mutation epoch: the inner reset() must
    // not wipe the outer's accumulated `mutated_`, and only the outermost
    // dispatch consumes it (T-DISPATCH-REENTRANT-1, story 7.7.1). Paired via
    // the controller's DispatchScope RAII guard.
    void begin_dispatch() { ++dispatch_depth_; }
    void end_dispatch() {
        if (dispatch_depth_ > 0) --dispatch_depth_;
    }

    // Notified when JS calls element.focus()/blur(), so the caller can route the
    // request to the input controller's caret target. Persists across reset().
    using FocusSink = std::function<void(DOM::Element*, bool /*focused*/)>;
    void set_focus_sink(FocusSink sink) { focus_sink_ = std::move(sink); }

    // fetch (9.1.1). Supplied by the Tab, which is the only layer that knows both
    // the resource loader and the current document URL. Returns the request id,
    // or 0 if it could not be started. Persists across reset().
    using FetchSink = std::function<std::uint64_t(const ScriptFetchRequest&)>;
    void set_fetch_sink(FetchSink sink) { fetch_sink_ = std::move(sink); }
    // Resolves a relative URL against the current document. Also from the Tab.
    using UrlResolver = std::function<std::string(std::string_view)>;
    void set_url_resolver(UrlResolver resolver) { url_resolver_ = std::move(resolver); }

    std::uint64_t start_fetch(const ScriptFetchRequest& request) override {
        return fetch_sink_ ? fetch_sink_(request) : 0;
    }
    std::string resolve_url(std::string_view relative) override {
        return url_resolver_ ? url_resolver_(relative) : std::string(relative);
    }

    // document.cookie (8.1.5). Supplied by the owner rather than held directly,
    // because the jar is shared per profile while the document URL changes per
    // navigation, and only the Tab knows both. Persists across reset().
    using CookieReader = std::function<std::string()>;
    using CookieWriter = std::function<void(std::string_view)>;
    void set_cookie_accessors(CookieReader reader, CookieWriter writer) {
        cookie_reader_ = std::move(reader);
        cookie_writer_ = std::move(writer);
    }

    // localStorage (8.2.2): supplied by the Tab, which is the only layer that
    // knows both the profile's storage manager and the current document's
    // origin. Returns the StorageArea for that origin, or nullptr when there is
    // none (opaque origin, or no manager) — the host degrades to empty reads and
    // dropped writes. Persists across reset().
    using StorageAccessor = std::function<Core::StorageArea*()>;
    void set_storage_accessor(StorageAccessor accessor) { storage_accessor_ = std::move(accessor); }
    // sessionStorage (8.2.3): a per-tab, never-persisted area. Same accessor
    // shape; the Tab supplies a store keyed off its own in-memory session map.
    void set_session_storage_accessor(StorageAccessor accessor) { session_storage_accessor_ = std::move(accessor); }

    DOM::Element* get_element_by_id(std::string_view id) override;
    DOM::Element* document_part(DocumentPart part) override;
    std::string get_text_content(const DOM::Node* node) override;
    void set_text_content(DOM::Node* node, std::string_view text) override;
    void set_attribute(DOM::Node* node, std::string_view name, std::string_view value) override;

    bool has_attribute(DOM::Node* node, std::string_view name) override;
    std::string get_attribute(DOM::Node* node, std::string_view name) override;
    void remove_attribute(DOM::Node* node, std::string_view name) override;

    bool class_list_contains(DOM::Node* node, std::string_view token) override;
    void class_list_add(DOM::Node* node, std::string_view token) override;
    void class_list_remove(DOM::Node* node, std::string_view token) override;
    bool class_list_toggle(DOM::Node* node, std::string_view token) override;

    bool get_dataset(DOM::Node* node, std::string_view key, std::string& out) override;
    void set_dataset(DOM::Node* node, std::string_view key, std::string_view value) override;

    void set_inner_html(DOM::Node* node, std::string_view html) override;
    std::string get_inner_html(DOM::Node* node) override;

    std::string get_value(DOM::Node* node) override;
    void set_value(DOM::Node* node, std::string_view value) override;
    bool get_checked(DOM::Node* node) override;
    void set_checked(DOM::Node* node, bool checked) override;
    bool get_disabled(DOM::Node* node) override;
    void set_disabled(DOM::Node* node, bool disabled) override;
    void set_focused(DOM::Node* node, bool focused) override;

    std::string get_document_cookie() override;
    void set_document_cookie(std::string_view value) override;

    std::optional<std::string> storage_get_item(StorageKind kind, std::string_view key) override;
    StorageWriteResult storage_set_item(StorageKind kind, std::string_view key, std::string_view value) override;
    void storage_remove_item(StorageKind kind, std::string_view key) override;
    void storage_clear(StorageKind kind) override;
    size_t storage_length(StorageKind kind) override;
    std::optional<std::string> storage_key(StorageKind kind, size_t index) override;

    DOM::Node* query_selector(DOM::Node* scope, std::string_view selector) override;
    std::vector<DOM::Node*> query_selector_all(DOM::Node* scope, std::string_view selector) override;
    bool matches(DOM::Node* node, std::string_view selector) override;
    DOM::Node* closest(DOM::Node* node, std::string_view selector) override;
    std::vector<DOM::Node*> get_elements_by_class_name(DOM::Node* scope, std::string_view names) override;
    std::vector<DOM::Node*> get_elements_by_tag_name(DOM::Node* scope, std::string_view tag) override;

    DOM::Element* create_element(std::string_view tag_name) override;
    DOM::Node* create_text_node(std::string_view data) override;

    DOM::Node* append_child(DOM::Node* parent, DOM::Node* child) override;
    DOM::Node* insert_before(DOM::Node* parent, DOM::Node* child, DOM::Node* reference) override;
    DOM::Node* remove_child(DOM::Node* parent, DOM::Node* child) override;
    DOM::Node* replace_child(DOM::Node* parent, DOM::Node* new_child, DOM::Node* old_child) override;

    DOM::Node* parent_node(DOM::Node* node) override;
    DOM::Node* first_child(DOM::Node* node) override;
    DOM::Node* last_child(DOM::Node* node) override;
    DOM::Node* next_sibling(DOM::Node* node) override;
    DOM::Node* previous_sibling(DOM::Node* node) override;
    DOM::Node* next_element_sibling(DOM::Node* node) override;
    DOM::Node* previous_element_sibling(DOM::Node* node) override;
    std::vector<DOM::Node*> child_nodes(DOM::Node* node) override;
    std::vector<DOM::Node*> child_elements(DOM::Node* node) override;

    NodeKind node_kind(const DOM::Node* node) override;
    std::string node_name(const DOM::Node* node) override;

private:
    // Takes exclusive ownership of `node` out of wherever it currently lives
    // (its parent's child list or the detached set). Returns an empty pointer
    // when the node cannot be located or has no owning arena.
    Core::ArenaPtr<DOM::Node> take_ownership(DOM::Node* node);

    // Moves every child of `node` into the detached set instead of destroying it.
    // innerHTML / textContent replace a subtree, and either can run mid-event-
    // dispatch (a `change` handler that does `list.innerHTML = ''` also clears the
    // node the event is being dispatched to). Detaching keeps those nodes valid —
    // "removal detaches, never frees" — so the in-flight dispatch and the stale
    // render tree never dereference a destroyed node (dom_arena_ownership.md).
    void detach_children(DOM::Node* node);

    DOM::Node* root_ = nullptr;
    Core::ArenaAllocator* arena_ = nullptr;
    bool mutated_ = false;
    int dispatch_depth_ = 0;  // >1 while a dispatch is nested inside another
    FocusSink focus_sink_;
    FetchSink fetch_sink_;
    UrlResolver url_resolver_;
    // Resolves the live StorageArea for the requested Web Storage kind, or null.
    Core::StorageArea* storage_area_for(StorageKind kind);

    CookieReader cookie_reader_;
    CookieWriter cookie_writer_;
    StorageAccessor storage_accessor_;
    StorageAccessor session_storage_accessor_;

    // Nodes created by script but not yet attached, plus nodes removed from the
    // tree. Their arena storage stays valid until the arena resets, so they can
    // be re-inserted; this vector is the single owner while they are detached.
    std::vector<Core::ArenaPtr<DOM::Node>> detached_;
};

}  // namespace Hummingbird::Engine
