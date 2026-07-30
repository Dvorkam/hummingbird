#include <gtest/gtest.h>

#include <string>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "engine/document/DocumentModel.h"
#include "engine/script/DocumentScriptHost.h"

namespace {
using Hummingbird::NodeKind;
using Hummingbird::Core::ArenaAllocator;
using Hummingbird::DOM::Element;
using Hummingbird::DOM::Node;
using Hummingbird::DOM::Text;
using Hummingbird::Engine::DocumentScriptHost;
}  // namespace

TEST(DocumentScriptHostTest, FindsElementByIdAndUpdatesText) {
    ArenaAllocator arena(1024, 1);

    auto root = Element::create(arena, "div");
    auto child = Element::create(arena, "span");
    child->set_attribute("id", "target");
    child->append_child(Text::create(arena, "Hello"));
    root->append_child(std::move(child));

    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* found = host.get_element_by_id("target");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(host.get_text_content(found), "Hello");

    host.set_text_content(found, "Updated");
    EXPECT_EQ(host.get_text_content(found), "Updated");
    EXPECT_TRUE(host.consume_mutations());
    EXPECT_FALSE(host.consume_mutations());
}

TEST(DocumentScriptHostTest, CreatesAndAppendsElementSubtree) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "ul");

    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    // Build <li>One</li> from scratch and attach it.
    auto* li = host.create_element("li");
    ASSERT_NE(li, nullptr);
    auto* text = host.create_text_node("One");
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(host.append_child(li, text), text);
    EXPECT_EQ(host.append_child(root.get(), li), li);

    ASSERT_EQ(root->get_children().size(), 1u);
    EXPECT_EQ(root->get_children().front().get(), li);
    EXPECT_EQ(host.get_text_content(root.get()), "One");
    EXPECT_EQ(host.parent_node(li), root.get());
    EXPECT_TRUE(host.consume_mutations());
}

TEST(DocumentScriptHostTest, InsertBeforeOrdersChildren) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "ul");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* a = host.create_element("li");
    auto* b = host.create_element("li");
    auto* c = host.create_element("li");
    host.append_child(root.get(), a);
    host.append_child(root.get(), c);
    // Insert b between a and c.
    EXPECT_EQ(host.insert_before(root.get(), b, c), b);

    ASSERT_EQ(root->get_children().size(), 3u);
    EXPECT_EQ(root->get_children()[0].get(), a);
    EXPECT_EQ(root->get_children()[1].get(), b);
    EXPECT_EQ(root->get_children()[2].get(), c);

    // Null reference appends.
    auto* d = host.create_element("li");
    EXPECT_EQ(host.insert_before(root.get(), d, nullptr), d);
    EXPECT_EQ(root->get_children().back().get(), d);
}

TEST(DocumentScriptHostTest, InsertBeforeSelfIsNoOp) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "ul");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* a = host.create_element("li");
    auto* b = host.create_element("li");
    auto* c = host.create_element("li");
    host.append_child(root.get(), a);
    host.append_child(root.get(), b);
    host.append_child(root.get(), c);

    // node.insertBefore(node, node) must be a no-op (spec: the reference is
    // re-targeted to node's next sibling before the move), not drop the node.
    EXPECT_EQ(host.insert_before(root.get(), b, b), b);

    ASSERT_EQ(root->get_children().size(), 3u);
    EXPECT_EQ(root->get_children()[0].get(), a);
    EXPECT_EQ(root->get_children()[1].get(), b);
    EXPECT_EQ(root->get_children()[2].get(), c);
    EXPECT_EQ(host.parent_node(b), root.get());

    // Same case when node is the last child: reference becomes null (append),
    // which must also land the node back in the same place.
    EXPECT_EQ(host.insert_before(root.get(), c, c), c);
    ASSERT_EQ(root->get_children().size(), 3u);
    EXPECT_EQ(root->get_children()[2].get(), c);
}

TEST(DocumentScriptHostTest, RemoveThenReinsertKeepsNodeValid) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "ul");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* li = host.create_element("li");
    host.append_child(li, host.create_text_node("keep"));
    host.append_child(root.get(), li);
    ASSERT_EQ(root->get_children().size(), 1u);

    // Remove detaches without freeing: the node and its subtree survive.
    EXPECT_EQ(host.remove_child(root.get(), li), li);
    EXPECT_EQ(root->get_children().size(), 0u);
    EXPECT_EQ(host.parent_node(li), nullptr);
    EXPECT_EQ(host.get_text_content(li), "keep");

    // Reinsert the same node; the tree is intact and text preserved.
    EXPECT_EQ(host.append_child(root.get(), li), li);
    ASSERT_EQ(root->get_children().size(), 1u);
    EXPECT_EQ(host.get_text_content(root.get()), "keep");
}

TEST(DocumentScriptHostTest, DetachedNodesSurviveRebindToSameDocument) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "ul");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* li = host.create_element("li");
    host.append_child(li, host.create_text_node("keep"));
    host.append_child(root.get(), li);
    EXPECT_EQ(host.remove_child(root.get(), li), li);

    // Every event dispatch rebinds the host (bind_host -> reset). Rebinding to
    // the same document must not destroy detached nodes: JS may still hold a
    // wrapper and re-insert the node in a later handler (todo.js's delete +
    // undo pattern; "removal detaches, never frees").
    host.reset(root.get(), &arena);
    EXPECT_EQ(host.get_text_content(li), "keep");
    EXPECT_EQ(host.append_child(root.get(), li), li);
    ASSERT_EQ(root->get_children().size(), 1u);
    EXPECT_EQ(host.get_text_content(root.get()), "keep");
}

TEST(DocumentScriptHostTest, AppendMovesNodeFromOldParent) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "div");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* src = host.create_element("div");
    auto* dst = host.create_element("div");
    auto* moved = host.create_element("span");
    host.append_child(root.get(), src);
    host.append_child(root.get(), dst);
    host.append_child(src, moved);
    ASSERT_EQ(src->get_children().size(), 1u);

    // Re-appending to another parent removes it from the first.
    EXPECT_EQ(host.append_child(dst, moved), moved);
    EXPECT_EQ(src->get_children().size(), 0u);
    ASSERT_EQ(dst->get_children().size(), 1u);
    EXPECT_EQ(dst->get_children().front().get(), moved);
    EXPECT_EQ(host.parent_node(moved), dst);
}

TEST(DocumentScriptHostTest, ReplaceChildSwapsNodes) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "ul");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* old_li = host.create_element("li");
    auto* new_li = host.create_element("li");
    host.append_child(root.get(), old_li);

    EXPECT_EQ(host.replace_child(root.get(), new_li, old_li), old_li);
    ASSERT_EQ(root->get_children().size(), 1u);
    EXPECT_EQ(root->get_children().front().get(), new_li);
    EXPECT_EQ(host.parent_node(old_li), nullptr);
}

TEST(DocumentScriptHostTest, RejectsHierarchyViolations) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "div");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* parent = host.create_element("div");
    auto* child = host.create_element("div");
    host.append_child(root.get(), parent);
    host.append_child(parent, child);

    // Appending an ancestor into its own descendant must be rejected.
    EXPECT_EQ(host.append_child(child, parent), nullptr);
    // Appending a node to itself is rejected too.
    EXPECT_EQ(host.append_child(parent, parent), nullptr);
    // A text node cannot host children.
    auto* text = host.create_text_node("leaf");
    EXPECT_EQ(host.append_child(text, host.create_element("span")), nullptr);
}

TEST(DocumentScriptHostTest, AttributeGetRemoveRoundTrip) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "div");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* el = host.create_element("a");
    host.append_child(root.get(), el);

    EXPECT_FALSE(host.has_attribute(el, "href"));
    host.set_attribute(el, "href", "/x");
    EXPECT_TRUE(host.has_attribute(el, "href"));
    EXPECT_EQ(host.get_attribute(el, "href"), "/x");
    // Attribute names are ASCII case-insensitive.
    EXPECT_TRUE(host.has_attribute(el, "HREF"));

    host.remove_attribute(el, "href");
    EXPECT_FALSE(host.has_attribute(el, "href"));
    EXPECT_EQ(host.get_attribute(el, "href"), "");
}

TEST(DocumentScriptHostTest, ClassListAddRemoveToggleContains) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "div");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* el = host.create_element("li");
    host.append_child(root.get(), el);

    host.class_list_add(el, "todo");
    host.class_list_add(el, "todo");  // idempotent
    host.class_list_add(el, "active");
    EXPECT_EQ(host.get_attribute(el, "class"), "todo active");
    EXPECT_TRUE(host.class_list_contains(el, "todo"));
    EXPECT_FALSE(host.class_list_contains(el, "done"));

    EXPECT_TRUE(host.class_list_toggle(el, "done"));  // absent -> added
    EXPECT_TRUE(host.class_list_contains(el, "done"));
    EXPECT_FALSE(host.class_list_toggle(el, "done"));  // present -> removed
    EXPECT_FALSE(host.class_list_contains(el, "done"));

    host.class_list_remove(el, "todo");
    EXPECT_EQ(host.get_attribute(el, "class"), "active");
}

TEST(DocumentScriptHostTest, DatasetMapsCamelCaseToDataAttr) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "div");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* el = host.create_element("li");
    host.append_child(root.get(), el);

    host.set_dataset(el, "id", "42");
    host.set_dataset(el, "userName", "kate");
    EXPECT_EQ(host.get_attribute(el, "data-id"), "42");
    EXPECT_EQ(host.get_attribute(el, "data-user-name"), "kate");

    std::string out;
    EXPECT_TRUE(host.get_dataset(el, "id", out));
    EXPECT_EQ(out, "42");
    EXPECT_TRUE(host.get_dataset(el, "userName", out));
    EXPECT_EQ(out, "kate");
    EXPECT_FALSE(host.get_dataset(el, "missing", out));
}

TEST(DocumentScriptHostTest, FormControlSurfaceReflectsState) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "form");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* field = host.create_element("input");
    host.append_child(root.get(), field);

    // value <-> value attribute
    EXPECT_EQ(host.get_value(field), "");
    host.set_value(field, "hello");
    EXPECT_EQ(host.get_value(field), "hello");
    EXPECT_EQ(host.get_attribute(field, "value"), "hello");

    // checked <-> presence of the checked attribute (MVP)
    auto* box = host.create_element("input");
    host.set_attribute(box, "type", "checkbox");
    host.append_child(root.get(), box);
    EXPECT_FALSE(host.get_checked(box));
    host.set_checked(box, true);
    EXPECT_TRUE(host.get_checked(box));
    EXPECT_TRUE(host.has_attribute(box, "checked"));
    host.set_checked(box, false);
    EXPECT_FALSE(host.has_attribute(box, "checked"));

    // disabled
    host.set_disabled(field, true);
    EXPECT_TRUE(host.get_disabled(field));
    host.set_disabled(field, false);
    EXPECT_FALSE(host.get_disabled(field));

    // focus() reflects the :focus pseudo-state
    EXPECT_FALSE(field->has_pseudo_state(Element::PseudoState::Focus));
    host.set_focused(field, true);
    EXPECT_TRUE(field->has_pseudo_state(Element::PseudoState::Focus));
    host.set_focused(field, false);
    EXPECT_FALSE(field->has_pseudo_state(Element::PseudoState::Focus));
    EXPECT_TRUE(host.consume_mutations());
}

TEST(DocumentScriptHostTest, InnerHtmlParsesFragmentReplacesAndSerializes) {
    ArenaAllocator arena(8192, 8);
    auto root = Element::create(arena, "div");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* ul = host.create_element("ul");
    host.append_child(ul, host.create_text_node("stale"));  // existing content to replace
    host.append_child(root.get(), ul);

    host.set_inner_html(ul, "<li class=\"a\">one</li><li>two</li>");
    auto kids = host.child_elements(ul);
    ASSERT_EQ(kids.size(), 2u);
    EXPECT_EQ(host.get_attribute(kids[0], "class"), "a");
    EXPECT_EQ(host.get_text_content(kids[0]), "one");
    EXPECT_EQ(host.get_text_content(kids[1]), "two");
    EXPECT_EQ(host.parent_node(kids[0]), ul);
    EXPECT_TRUE(host.consume_mutations());

    // Round-trips through serialization (single attribute -> deterministic order).
    EXPECT_EQ(host.get_inner_html(ul), "<li class=\"a\">one</li><li>two</li>");
}

TEST(DocumentScriptHostTest, InnerHtmlRecoversFromMalformedAndEscapesText) {
    ArenaAllocator arena(8192, 8);
    auto root = Element::create(arena, "div");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto* ul = host.create_element("ul");
    host.append_child(root.get(), ul);

    // Unclosed <li>s recover into siblings, matching document parsing.
    host.set_inner_html(ul, "<li>a<li>b");
    EXPECT_EQ(host.child_elements(ul).size(), 2u);

    // Text is escaped on the way out; void elements emit no end tag.
    auto* p = host.create_element("p");
    host.append_child(root.get(), p);
    host.set_inner_html(p, "x &amp; y<br>z");
    EXPECT_EQ(host.get_inner_html(p), "x &amp; y<br>z");
}

TEST(DocumentScriptHostTest, SelectorQueriesMatchLikeTheStyleEngine) {
    ArenaAllocator arena(8192, 8);
    DocumentScriptHost host;

    // <div id=root><ul class="list">
    //   <li class="item active"><span>a</span></li>
    //   <li class="item"><span>b</span></li>
    // </ul></div>
    auto root = Element::create(arena, "div");
    host.reset(root.get(), &arena);

    auto* list = host.create_element("ul");
    host.set_attribute(list, "class", "list");
    host.append_child(root.get(), list);

    auto* li1 = host.create_element("li");
    host.set_attribute(li1, "class", "item active");
    auto* span1 = host.create_element("span");
    host.append_child(span1, host.create_text_node("a"));
    host.append_child(li1, span1);
    host.append_child(list, li1);

    auto* li2 = host.create_element("li");
    host.set_attribute(li2, "class", "item");
    auto* span2 = host.create_element("span");
    host.append_child(span2, host.create_text_node("b"));
    host.append_child(li2, span2);
    host.append_child(list, li2);

    // Document-scoped (nullptr) queries.
    EXPECT_EQ(host.query_selector(nullptr, "li.active"), li1);
    EXPECT_EQ(host.query_selector(nullptr, ".missing"), nullptr);
    EXPECT_EQ(host.query_selector_all(nullptr, "li").size(), 2u);
    EXPECT_EQ(host.query_selector_all(nullptr, ".item span").size(), 2u);
    EXPECT_EQ(host.get_elements_by_class_name(nullptr, "item").size(), 2u);
    EXPECT_EQ(host.get_elements_by_class_name(nullptr, "item active").size(), 1u);
    EXPECT_EQ(host.get_elements_by_tag_name(nullptr, "span").size(), 2u);
    EXPECT_EQ(host.get_elements_by_tag_name(nullptr, "*").size(), 5u);  // ul,li,span,li,span

    // matches / closest.
    EXPECT_TRUE(host.matches(li1, "li.active"));
    EXPECT_FALSE(host.matches(li2, ".active"));
    EXPECT_EQ(host.closest(span1, "ul"), list);
    EXPECT_EQ(host.closest(span1, ".active"), li1);
    EXPECT_EQ(host.closest(span1, "table"), nullptr);

    // Element-scoped query searches only that element's descendants.
    EXPECT_EQ(host.query_selector_all(li2, "span").size(), 1u);
    EXPECT_EQ(host.query_selector(li2, "span"), span2);
}

TEST(DocumentScriptHostTest, TraversalAccessorsSkipTextForElementSiblings) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "p");
    DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* first = host.create_element("a");
    auto* between = host.create_text_node(" and ");
    auto* second = host.create_element("b");
    host.append_child(root.get(), first);
    host.append_child(root.get(), between);
    host.append_child(root.get(), second);

    EXPECT_EQ(host.first_child(root.get()), first);
    EXPECT_EQ(host.last_child(root.get()), second);
    EXPECT_EQ(host.next_sibling(first), between);
    EXPECT_EQ(host.next_element_sibling(first), second);
    EXPECT_EQ(host.previous_element_sibling(second), first);
    EXPECT_EQ(host.previous_sibling(second), between);

    EXPECT_EQ(host.child_nodes(root.get()).size(), 3u);
    EXPECT_EQ(host.child_elements(root.get()).size(), 2u);
    EXPECT_EQ(host.node_kind(first), NodeKind::Element);
    EXPECT_EQ(host.node_kind(between), NodeKind::Text);
    EXPECT_EQ(host.node_name(first), "A");
    EXPECT_EQ(host.node_name(between), "#text");
}

// T-DOM-DOCUMENT-BODY-1. Driven through the REAL parser, not a hand-built tree:
// the whole question this API answers is what shape the parser produces
// (`root` wrapper -> <html> -> <head>/<body>), so a hand-assembled tree would
// only confirm the assumption instead of testing it.
TEST(DocumentScriptHostTest, ResolvesTheDocumentPartsFromParsedHtml) {
    Hummingbird::Engine::DocumentModel model;
    ASSERT_TRUE(model.parse_html("<html><head><title>t</title></head>"
                                 "<body><p id='hi'>hello</p></body></html>")
                    .ok);

    ArenaAllocator arena(4096, 4);
    DocumentScriptHost host;
    host.reset(model.dom_root(), &arena);

    auto* document_element = host.document_part(DocumentScriptHost::DocumentPart::DocumentElement);
    ASSERT_NE(document_element, nullptr);
    EXPECT_EQ(host.node_name(document_element), "HTML");

    auto* body = host.document_part(DocumentScriptHost::DocumentPart::Body);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(host.node_name(body), "BODY");
    EXPECT_EQ(host.get_text_content(body), "hello") << "the body must be the real one, holding the page content";

    auto* head = host.document_part(DocumentScriptHost::DocumentPart::Head);
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(host.node_name(head), "HEAD");

    // The parts are the same objects the rest of the API hands out, so a page
    // can compare them by identity the way `el.parentNode === document.body` does.
    auto* paragraph = host.get_element_by_id("hi");
    ASSERT_NE(paragraph, nullptr);
    EXPECT_EQ(host.parent_node(paragraph), body);
}

// A frameset document has no <body> at all. Returning null there would read as
// "no document"; the spec says documentElement's frameset child IS the body.
TEST(DocumentScriptHostTest, DocumentBodyFallsBackToFramesetPerTheSpec) {
    Hummingbird::Engine::DocumentModel model;
    ASSERT_TRUE(model.parse_html("<html><head></head><frameset><frame></frameset></html>").ok);

    ArenaAllocator arena(4096, 4);
    DocumentScriptHost host;
    host.reset(model.dom_root(), &arena);

    auto* body = host.document_part(DocumentScriptHost::DocumentPart::Body);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(host.node_name(body), "FRAMESET");
}

// The honest null. This engine's parser does not synthesize the html/head/body
// skeleton that a browser's tree construction would, so a document written
// without those tags genuinely has none — and the binding says so rather than
// substituting the root, which would make `document.body.tagName` a lie.
// Tracked as T-HTML-TREE-SKELETON-1.
TEST(DocumentScriptHostTest, DocumentPartsAreNullWhenTheMarkupOmitsThem) {
    Hummingbird::Engine::DocumentModel model;
    ASSERT_TRUE(model.parse_html("<p>bare fragment</p>").ok);

    ArenaAllocator arena(4096, 4);
    DocumentScriptHost host;
    host.reset(model.dom_root(), &arena);

    EXPECT_EQ(host.document_part(DocumentScriptHost::DocumentPart::DocumentElement), nullptr);
    EXPECT_EQ(host.document_part(DocumentScriptHost::DocumentPart::Body), nullptr);
    EXPECT_EQ(host.document_part(DocumentScriptHost::DocumentPart::Head), nullptr);
}

// No host bound at all: every part is null rather than a crash.
TEST(DocumentScriptHostTest, DocumentPartsAreNullWithoutADocument) {
    DocumentScriptHost host;
    EXPECT_EQ(host.document_part(DocumentScriptHost::DocumentPart::Body), nullptr);
}
