#include "engine/script/DocumentScriptHost.h"

#include <utility>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagMetadata.h"
#include "style/compute/Stylesheet.h"
#include "style/parser/CssParser.h"
#include "style/selector/SelectorMatcher.h"

namespace Hummingbird::Engine {

namespace {
DOM::Element* find_element_by_id(DOM::Node* node, std::string_view id) {
    if (!node) return nullptr;

    if (auto* element = dynamic_cast<DOM::Element*>(node)) {
        if (const auto* attr = element->find_attribute(Hummingbird::Html::AttributeNames::Id)) {
            if (*attr == id) {
                return element;
            }
        }
    }

    for (const auto& child : node->get_children()) {
        if (auto* match = find_element_by_id(child.get(), id)) {
            return match;
        }
    }
    return nullptr;
}

// First direct child element with the given (lower-case) tag name. Direct
// children only, deliberately: `document.body` means "the body child of the
// html element", not "the first body anywhere", so a stray <body> nested in
// the content of a malformed page must not be mistaken for the real one.
DOM::Element* first_child_element_named(DOM::Node* parent, std::string_view tag) {
    if (!parent) return nullptr;
    for (const auto& child : parent->get_children()) {
        if (auto* element = dynamic_cast<DOM::Element*>(child.get())) {
            if (element->get_tag_name() == tag) {
                return element;
            }
        }
    }
    return nullptr;
}

void collect_text(const DOM::Node* node, std::string& out) {
    if (!node) return;
    if (auto* text_node = dynamic_cast<const DOM::Text*>(node)) {
        out.append(text_node->get_text());
        return;
    }
    for (const auto& child : node->get_children()) {
        collect_text(child.get(), out);
    }
}

// Maps a dataset property name to its data-* attribute name per the HTML rules:
// prepend "data-" and turn each ASCII uppercase letter into "-" + lowercase
// ("userId" -> "data-user-id").
std::string dataset_key_to_attr(std::string_view key) {
    std::string attr = "data-";
    for (char c : key) {
        if (c >= 'A' && c <= 'Z') {
            attr.push_back('-');
            attr.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
            attr.push_back(c);
        }
    }
    return attr;
}

// Appends `text` to `out`, escaping the characters that are unsafe in HTML text
// (or, when `in_attribute`, in a double-quoted attribute value).
void append_escaped(std::string& out, std::string_view text, bool in_attribute) {
    for (char c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                if (in_attribute) {
                    out += "&quot;";
                } else {
                    out.push_back(c);
                }
                break;
            default:
                out.push_back(c);
        }
    }
}

void serialize_node(const DOM::Node* node, std::string& out) {
    if (const auto* text = dynamic_cast<const DOM::Text*>(node)) {
        append_escaped(out, text->get_text(), /*in_attribute=*/false);
        return;
    }
    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (!element) {
        return;
    }
    const std::string& tag = element->get_tag_name();
    out.push_back('<');
    out += tag;
    for (const auto& [name, value] : element->get_attributes()) {
        out.push_back(' ');
        out += name;
        out += "=\"";
        append_escaped(out, value, /*in_attribute=*/true);
        out.push_back('"');
    }
    out.push_back('>');
    if (Html::TagMetadata::is_void_tag(tag)) {
        return;  // void elements have no children and no end tag
    }
    for (const auto& child : element->get_children()) {
        serialize_node(child.get(), out);
    }
    out += "</";
    out += tag;
    out.push_back('>');
}

// Parses a selector string into the style engine's selector list by reusing the
// CSS parser on a rule with an empty body. This keeps the querySelector-supported
// subset identical to what the style engine matches (7.1.3 scope).
std::vector<Css::Selector> parse_selector_list(std::string_view selector) {
    std::string css(selector);
    css += "{}";
    Css::Parser parser(css);
    Css::Stylesheet sheet = parser.parse();
    if (sheet.rules.empty()) {
        return {};
    }
    return std::move(sheet.rules.front().selectors);
}

bool any_selector_matches(const DOM::Node* node, const std::vector<Css::Selector>& selectors) {
    for (const auto& selector : selectors) {
        if (Css::matches_selector(node, selector)) {
            return true;
        }
    }
    return false;
}

// Pre-order (document-order) search of `scope`'s descendants for the first
// element matching any selector.
DOM::Node* find_first_descendant(DOM::Node* scope, const std::vector<Css::Selector>& selectors) {
    for (const auto& child : scope->get_children()) {
        DOM::Node* node = child.get();
        if (dynamic_cast<DOM::Element*>(node) && any_selector_matches(node, selectors)) {
            return node;
        }
        if (DOM::Node* found = find_first_descendant(node, selectors)) {
            return found;
        }
    }
    return nullptr;
}

void collect_matching_descendants(DOM::Node* scope, const std::vector<Css::Selector>& selectors,
                                  std::vector<DOM::Node*>& out) {
    for (const auto& child : scope->get_children()) {
        DOM::Node* node = child.get();
        if (dynamic_cast<DOM::Element*>(node) && any_selector_matches(node, selectors)) {
            out.push_back(node);
        }
        collect_matching_descendants(node, selectors, out);
    }
}

// Collects descendant elements satisfying an arbitrary predicate, document order.
template <typename Pred>
void collect_descendant_elements(DOM::Node* scope, Pred pred, std::vector<DOM::Node*>& out) {
    for (const auto& child : scope->get_children()) {
        DOM::Node* node = child.get();
        if (auto* element = dynamic_cast<DOM::Element*>(node); element && pred(*element)) {
            out.push_back(node);
        }
        collect_descendant_elements(node, pred, out);
    }
}

// Walks siblings from `node` in the given direction until an element is found.
DOM::Node* element_sibling(DOM::Node* node, bool forward) {
    if (!node) return nullptr;
    DOM::Node* cursor = forward ? node->next_sibling() : node->previous_sibling();
    while (cursor) {
        if (dynamic_cast<DOM::Element*>(cursor)) {
            return cursor;
        }
        cursor = forward ? cursor->next_sibling() : cursor->previous_sibling();
    }
    return nullptr;
}
}  // namespace

DocumentScriptHost::DocumentScriptHost() = default;
DocumentScriptHost::~DocumentScriptHost() = default;

void DocumentScriptHost::reset(DOM::Node* root, Core::ArenaAllocator* arena) {
    // Rebinding to the same live document (every event dispatch does this via
    // bind_host) must keep the detached set: JS may still hold wrappers to
    // removed nodes and re-insert them later — "removal detaches, never frees"
    // (doc/dev_guide/dom_arena_ownership.md). Only an actual document change
    // may drop them (their arena is torn down via clear() on navigation).
    if (root != root_ || arena != arena_) {
        detached_.clear();
    }
    root_ = root;
    arena_ = arena;
    // A nested dispatch's rebind must preserve the outer dispatch's mutation
    // epoch — only the outermost bind (depth 0 direct use, or 1 inside a
    // DispatchScope) starts fresh (T-DISPATCH-REENTRANT-1).
    if (dispatch_depth_ <= 1) {
        mutated_ = false;
    }
}

void DocumentScriptHost::clear() {
    root_ = nullptr;
    arena_ = nullptr;
    mutated_ = false;
    detached_.clear();
}

bool DocumentScriptHost::consume_mutations() {
    // A nested dispatch does not drain the flag: the outermost dispatch reports
    // and clears the accumulated mutations for the whole re-entrant chain.
    // Safe today because every nested caller (see fire_focus_transition's
    // re-entrant dispatch) discards this return value — a future nested caller
    // that inspects it directly would always see false, even if the nested
    // dispatch mutated the DOM.
    if (dispatch_depth_ >= 2) {
        return false;
    }
    bool result = mutated_;
    mutated_ = false;
    return result;
}

DOM::Element* DocumentScriptHost::get_element_by_id(std::string_view id) {
    if (!root_ || id.empty()) {
        return nullptr;
    }
    return find_element_by_id(root_, id);
}

DOM::Element* DocumentScriptHost::document_part(DocumentPart part) {
    if (!root_) {
        return nullptr;
    }
    // `root_` is the parser's synthetic "root" wrapper, not the document
    // element, so <html> is looked up one level down rather than assumed.
    DOM::Element* document_element = first_child_element_named(root_, "html");
    if (part == DocumentPart::DocumentElement) {
        return document_element;
    }
    if (!document_element) {
        return nullptr;
    }
    if (part == DocumentPart::Head) {
        return first_child_element_named(document_element, "head");
    }
    // Per the HTML spec `document.body` is the first body OR frameset child —
    // a frameset document has no <body> at all, and returning null there would
    // read as "no document" rather than "a different kind of document".
    if (auto* body = first_child_element_named(document_element, "body")) {
        return body;
    }
    return first_child_element_named(document_element, "frameset");
}

std::string DocumentScriptHost::get_text_content(const DOM::Node* node) {
    if (!node) return {};
    std::string text;
    collect_text(node, text);
    return text;
}

void DocumentScriptHost::set_text_content(DOM::Node* node, std::string_view text) {
    if (!node) return;
    if (auto* text_node = dynamic_cast<DOM::Text*>(node)) {
        text_node->set_text(text);
        mutated_ = true;
        return;
    }
    if (!arena_) return;
    detach_children(node);
    if (!text.empty()) {
        node->append_child(DOM::Text::create(*arena_, text));
    }
    mutated_ = true;
}

void DocumentScriptHost::detach_children(DOM::Node* node) {
    if (!node) return;
    while (DOM::Node* first = node->first_child()) {
        Core::ArenaPtr<DOM::Node> owned = node->remove_child_node(first);
        if (!owned) break;  // defensive: first_child() said there is one
        detached_.emplace_back(std::move(owned));
    }
}

void DocumentScriptHost::set_attribute(DOM::Node* node, std::string_view name, std::string_view value) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return;
    element->set_attribute(name, value);
    mutated_ = true;
}

bool DocumentScriptHost::has_attribute(DOM::Node* node, std::string_view name) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    return element && element->has_attribute(Core::Utils::to_lower(name));
}

std::string DocumentScriptHost::get_attribute(DOM::Node* node, std::string_view name) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return {};
    const auto* value = element->find_attribute(Core::Utils::to_lower(name));
    return value ? *value : std::string{};
}

void DocumentScriptHost::remove_attribute(DOM::Node* node, std::string_view name) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return;
    element->remove_attribute(name);
    mutated_ = true;
}

bool DocumentScriptHost::class_list_contains(DOM::Node* node, std::string_view token) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    return element && element->class_contains(token);
}

void DocumentScriptHost::class_list_add(DOM::Node* node, std::string_view token) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (element && element->class_add(token)) {
        mutated_ = true;
    }
}

void DocumentScriptHost::class_list_remove(DOM::Node* node, std::string_view token) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (element && element->class_remove(token)) {
        mutated_ = true;
    }
}

bool DocumentScriptHost::class_list_toggle(DOM::Node* node, std::string_view token) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return false;
    if (element->class_contains(token)) {
        if (element->class_remove(token)) mutated_ = true;
        return false;
    }
    if (element->class_add(token)) mutated_ = true;
    return true;
}

bool DocumentScriptHost::get_dataset(DOM::Node* node, std::string_view key, std::string& out) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return false;
    const auto* value = element->find_attribute(dataset_key_to_attr(key));
    if (!value) return false;
    out = *value;
    return true;
}

void DocumentScriptHost::set_dataset(DOM::Node* node, std::string_view key, std::string_view value) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return;
    element->set_attribute(dataset_key_to_attr(key), value);
    mutated_ = true;
}

void DocumentScriptHost::set_inner_html(DOM::Node* node, std::string_view html) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element || !arena_) return;

    // Reuse the document parser on the fragment: it appends top-level tags under
    // a synthetic root without synthesizing <html>/<body>, so the root's children
    // are exactly the fragment's nodes.
    Html::Parser parser(*arena_, html);
    auto parsed = parser.parse();

    detach_children(node);
    if (parsed.dom) {
        // Transfer parsed nodes in order; the synthetic root is left empty and
        // reclaimed with the arena.
        while (DOM::Node* first = parsed.dom->first_child()) {
            Core::ArenaPtr<DOM::Node> child = parsed.dom->remove_child_node(first);
            if (child) {
                node->append_child_node(std::move(child));
            }
        }
    }
    mutated_ = true;
}

std::string DocumentScriptHost::get_inner_html(DOM::Node* node) {
    if (!node) return {};
    std::string out;
    for (const auto& child : node->get_children()) {
        serialize_node(child.get(), out);
    }
    return out;
}

std::string DocumentScriptHost::get_value(DOM::Node* node) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return {};
    const auto* value = element->find_attribute(Hummingbird::Html::AttributeNames::Value);
    return value ? *value : std::string{};
}

void DocumentScriptHost::set_value(DOM::Node* node, std::string_view value) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return;
    element->set_attribute(Hummingbird::Html::AttributeNames::Value, value);
    mutated_ = true;
}

bool DocumentScriptHost::get_checked(DOM::Node* node) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    // MVP: checkedness is reflected by the presence of the `checked` attribute.
    return element && element->has_attribute("checked");
}

void DocumentScriptHost::set_checked(DOM::Node* node, bool checked) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return;
    if (checked) {
        element->set_attribute("checked", "");
    } else {
        element->remove_attribute("checked");
    }
    mutated_ = true;
}

bool DocumentScriptHost::get_disabled(DOM::Node* node) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    return element && element->has_attribute("disabled");
}

void DocumentScriptHost::set_disabled(DOM::Node* node, bool disabled) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return;
    if (disabled) {
        element->set_attribute("disabled", "");
    } else {
        element->remove_attribute("disabled");
    }
    mutated_ = true;
}

void DocumentScriptHost::set_focused(DOM::Node* node, bool focused) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return;
    // Reflect focus as the :focus pseudo-state so styling responds...
    if (element->set_pseudo_state(DOM::Element::PseudoState::Focus, focused)) {
        mutated_ = true;
    }
    // ...and route the request to the input controller so an editable field
    // becomes the live caret target (7.2.6).
    if (focus_sink_) {
        focus_sink_(element, focused);
    }
}

std::string DocumentScriptHost::get_document_cookie() {
    // No jar wired up (most unit tests) reads as "no cookies", not an error:
    // document.cookie returning "" is a perfectly ordinary state.
    return cookie_reader_ ? cookie_reader_() : std::string{};
}

void DocumentScriptHost::set_document_cookie(std::string_view value) {
    if (cookie_writer_) {
        cookie_writer_(value);
    }
}

Core::StorageArea* DocumentScriptHost::storage_area_for(StorageKind kind) {
    const StorageAccessor& accessor = kind == StorageKind::Session ? session_storage_accessor_ : storage_accessor_;
    return accessor ? accessor() : nullptr;
}

std::optional<std::string> DocumentScriptHost::storage_get_item(StorageKind kind, std::string_view key) {
    Core::StorageArea* area = storage_area_for(kind);
    return area ? area->get_item(key) : std::nullopt;
}

DocumentScriptHost::StorageWriteResult DocumentScriptHost::storage_set_item(StorageKind kind, std::string_view key,
                                                                            std::string_view value) {
    Core::StorageArea* area = storage_area_for(kind);
    if (!area) {
        return StorageWriteResult::Ok;  // no store: drop the write, like cookies with no jar
    }
    return area->set_item(key, value) ? StorageWriteResult::Ok : StorageWriteResult::QuotaExceeded;
}

void DocumentScriptHost::storage_remove_item(StorageKind kind, std::string_view key) {
    if (Core::StorageArea* area = storage_area_for(kind)) {
        area->remove_item(key);
    }
}

void DocumentScriptHost::storage_clear(StorageKind kind) {
    if (Core::StorageArea* area = storage_area_for(kind)) {
        area->clear();
    }
}

size_t DocumentScriptHost::storage_length(StorageKind kind) {
    Core::StorageArea* area = storage_area_for(kind);
    return area ? area->length() : 0;
}

std::optional<std::string> DocumentScriptHost::storage_key(StorageKind kind, size_t index) {
    Core::StorageArea* area = storage_area_for(kind);
    return area ? area->key_at(index) : std::nullopt;
}

DOM::Node* DocumentScriptHost::query_selector(DOM::Node* scope, std::string_view selector) {
    DOM::Node* root = scope ? scope : root_;
    if (!root) return nullptr;
    auto selectors = parse_selector_list(selector);
    if (selectors.empty()) return nullptr;
    return find_first_descendant(root, selectors);
}

std::vector<DOM::Node*> DocumentScriptHost::query_selector_all(DOM::Node* scope, std::string_view selector) {
    std::vector<DOM::Node*> result;
    DOM::Node* root = scope ? scope : root_;
    if (!root) return result;
    auto selectors = parse_selector_list(selector);
    if (selectors.empty()) return result;
    collect_matching_descendants(root, selectors, result);
    return result;
}

bool DocumentScriptHost::matches(DOM::Node* node, std::string_view selector) {
    if (!dynamic_cast<DOM::Element*>(node)) return false;
    auto selectors = parse_selector_list(selector);
    return !selectors.empty() && any_selector_matches(node, selectors);
}

DOM::Node* DocumentScriptHost::closest(DOM::Node* node, std::string_view selector) {
    auto selectors = parse_selector_list(selector);
    if (selectors.empty()) return nullptr;
    for (DOM::Node* cursor = node; cursor; cursor = cursor->get_parent()) {
        if (dynamic_cast<DOM::Element*>(cursor) && any_selector_matches(cursor, selectors)) {
            return cursor;
        }
    }
    return nullptr;
}

std::vector<DOM::Node*> DocumentScriptHost::get_elements_by_class_name(DOM::Node* scope, std::string_view names) {
    std::vector<DOM::Node*> result;
    DOM::Node* root = scope ? scope : root_;
    if (!root) return result;
    // All space-separated tokens must be present (matches the DOM contract).
    std::vector<std::string> required;
    for (auto token : Core::Utils::split_ascii_whitespace(names)) {
        required.emplace_back(token);
    }
    if (required.empty()) return result;
    collect_descendant_elements(
        root,
        [&](const DOM::Element& element) {
            for (const auto& token : required) {
                if (!element.class_contains(token)) return false;
            }
            return true;
        },
        result);
    return result;
}

std::vector<DOM::Node*> DocumentScriptHost::get_elements_by_tag_name(DOM::Node* scope, std::string_view tag) {
    std::vector<DOM::Node*> result;
    DOM::Node* root = scope ? scope : root_;
    if (!root) return result;
    const bool all = tag == "*";
    const std::string lowered = Core::Utils::to_lower(tag);
    collect_descendant_elements(
        root, [&](const DOM::Element& element) { return all || element.get_tag_name() == lowered; }, result);
    return result;
}

DOM::Element* DocumentScriptHost::create_element(std::string_view tag_name) {
    if (!arena_) return nullptr;
    auto element = DOM::Element::create(*arena_, tag_name);
    if (!element) return nullptr;
    DOM::Element* raw = element.get();
    detached_.emplace_back(Core::ArenaPtr<DOM::Node>(element.release()));
    return raw;
}

DOM::Node* DocumentScriptHost::create_text_node(std::string_view data) {
    if (!arena_) return nullptr;
    auto text = DOM::Text::create(*arena_, data);
    if (!text) return nullptr;
    DOM::Node* raw = text.get();
    detached_.emplace_back(Core::ArenaPtr<DOM::Node>(text.release()));
    return raw;
}

Core::ArenaPtr<DOM::Node> DocumentScriptHost::take_ownership(DOM::Node* node) {
    if (!node) return {};
    if (DOM::Node* parent = node->get_parent()) {
        return parent->remove_child_node(node);
    }
    for (auto it = detached_.begin(); it != detached_.end(); ++it) {
        if (it->get() == node) {
            Core::ArenaPtr<DOM::Node> owned = std::move(*it);
            detached_.erase(it);
            return owned;
        }
    }
    return {};
}

DOM::Node* DocumentScriptHost::append_child(DOM::Node* parent, DOM::Node* child) {
    return insert_before(parent, child, nullptr);
}

DOM::Node* DocumentScriptHost::insert_before(DOM::Node* parent, DOM::Node* child, DOM::Node* reference) {
    // Only elements can host children (text/other nodes are leaves).
    if (!dynamic_cast<DOM::Element*>(parent) || !child) {
        return nullptr;
    }
    // A node cannot become its own descendant, and the reference must belong to
    // the parent when provided.
    if (child->is_inclusive_ancestor_of(parent)) {
        return nullptr;
    }
    if (reference && reference->get_parent() != parent) {
        return nullptr;
    }
    // Per spec: inserting a node before itself re-targets the reference to its
    // next sibling first — otherwise take_ownership() below detaches `child`
    // from `parent` before insert_child_before() looks for `reference` (== the
    // now-detached `child`), fails to find it, and drops the node on the floor.
    if (reference == child) {
        reference = child->next_sibling();
    }
    Core::ArenaPtr<DOM::Node> owned = take_ownership(child);
    if (!owned) {
        return nullptr;
    }
    if (!parent->insert_child_before(std::move(owned), reference)) {
        return nullptr;
    }
    mutated_ = true;
    return child;
}

DOM::Node* DocumentScriptHost::remove_child(DOM::Node* parent, DOM::Node* child) {
    if (!parent || !child || child->get_parent() != parent) {
        return nullptr;
    }
    Core::ArenaPtr<DOM::Node> owned = parent->remove_child_node(child);
    if (!owned) {
        return nullptr;
    }
    detached_.emplace_back(std::move(owned));
    mutated_ = true;
    return child;
}

DOM::Node* DocumentScriptHost::replace_child(DOM::Node* parent, DOM::Node* new_child, DOM::Node* old_child) {
    if (!dynamic_cast<DOM::Element*>(parent) || !new_child || !old_child) {
        return nullptr;
    }
    if (old_child->get_parent() != parent) {
        return nullptr;
    }
    if (new_child->is_inclusive_ancestor_of(parent)) {
        return nullptr;
    }
    // Insert the replacement immediately before the outgoing node, then detach
    // the outgoing node. new_child == old_child is a no-op that keeps old_child.
    if (new_child != old_child) {
        Core::ArenaPtr<DOM::Node> incoming = take_ownership(new_child);
        if (!incoming) {
            return nullptr;
        }
        if (!parent->insert_child_before(std::move(incoming), old_child)) {
            return nullptr;
        }
        Core::ArenaPtr<DOM::Node> outgoing = parent->remove_child_node(old_child);
        if (outgoing) {
            detached_.emplace_back(std::move(outgoing));
        }
    }
    mutated_ = true;
    return old_child;
}

DOM::Node* DocumentScriptHost::parent_node(DOM::Node* node) {
    return node ? node->get_parent() : nullptr;
}
DOM::Node* DocumentScriptHost::first_child(DOM::Node* node) {
    return node ? node->first_child() : nullptr;
}
DOM::Node* DocumentScriptHost::last_child(DOM::Node* node) {
    return node ? node->last_child() : nullptr;
}
DOM::Node* DocumentScriptHost::next_sibling(DOM::Node* node) {
    return node ? node->next_sibling() : nullptr;
}
DOM::Node* DocumentScriptHost::previous_sibling(DOM::Node* node) {
    return node ? node->previous_sibling() : nullptr;
}

DOM::Node* DocumentScriptHost::next_element_sibling(DOM::Node* node) {
    return element_sibling(node, /*forward=*/true);
}
DOM::Node* DocumentScriptHost::previous_element_sibling(DOM::Node* node) {
    return element_sibling(node, /*forward=*/false);
}

std::vector<DOM::Node*> DocumentScriptHost::child_nodes(DOM::Node* node) {
    std::vector<DOM::Node*> result;
    if (!node) return result;
    const auto& children = node->get_children();
    result.reserve(children.size());
    for (const auto& child : children) {
        result.push_back(child.get());
    }
    return result;
}

std::vector<DOM::Node*> DocumentScriptHost::child_elements(DOM::Node* node) {
    std::vector<DOM::Node*> result;
    if (!node) return result;
    for (const auto& child : node->get_children()) {
        if (dynamic_cast<DOM::Element*>(child.get())) {
            result.push_back(child.get());
        }
    }
    return result;
}

NodeKind DocumentScriptHost::node_kind(const DOM::Node* node) {
    if (dynamic_cast<const DOM::Element*>(node)) return NodeKind::Element;
    if (dynamic_cast<const DOM::Text*>(node)) return NodeKind::Text;
    return NodeKind::Other;
}

std::string DocumentScriptHost::node_name(const DOM::Node* node) {
    if (auto* element = dynamic_cast<const DOM::Element*>(node)) {
        return Core::Utils::to_upper(element->get_tag_name());
    }
    if (dynamic_cast<const DOM::Text*>(node)) {
        return "#text";
    }
    return {};
}

}  // namespace Hummingbird::Engine
