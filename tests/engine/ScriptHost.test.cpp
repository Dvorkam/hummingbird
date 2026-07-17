#include <gtest/gtest.h>

#include <string>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
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
