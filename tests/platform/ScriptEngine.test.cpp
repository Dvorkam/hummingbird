#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "engine/script/DocumentScriptHost.h"

TEST(ScriptEngineTest, EvalSucceedsInQuickJsEngine) {
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    auto result = engine->eval("1 + 1", "inline");
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.error.empty());
}

TEST(ScriptEngineTest, EvalReportsErrors) {
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    auto result = engine->eval("function {", "inline");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
}

TEST(ScriptEngineTest, EvalMutatesDomThroughBindings) {
    Hummingbird::Core::ArenaAllocator arena(1024, 1);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto target = Hummingbird::DOM::Element::create(arena, "span");
    target->set_attribute("id", "target");
    target->append_child(Hummingbird::DOM::Text::create(arena, "Hello"));
    root->append_child(std::move(target));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "const target = document.getElementById('target');"
        "if (target) {"
        "  target.textContent = 'Updated';"
        "  target.setAttribute('data-js', 'ok');"
        "}",
        "inline");
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.error.empty());

    auto* found = host.get_element_by_id("target");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(host.get_text_content(found), "Updated");
    const auto* attr = found->find_attribute("data-js");
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(*attr, "ok");
    EXPECT_TRUE(host.consume_mutations());
    EXPECT_FALSE(host.consume_mutations());
}

TEST(ScriptEngineTest, BuildsListFromScratchThroughBindings) {
    Hummingbird::Core::ArenaAllocator arena(8192, 8);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto container = Hummingbird::DOM::Element::create(arena, "ul");
    container->set_attribute("id", "list");
    root->append_child(std::move(container));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // JS builds three <li> children, then removes the middle one.
    auto result = engine->eval(
        "var list = document.getElementById('list');"
        "for (var i = 0; i < 3; i++) {"
        "  var li = document.createElement('li');"
        "  li.appendChild(document.createTextNode('item' + i));"
        "  list.appendChild(li);"
        "}"
        "list.removeChild(list.children[1]);",
        "inline");
    ASSERT_TRUE(result.ok) << result.error;

    auto* list = host.get_element_by_id("list");
    ASSERT_NE(list, nullptr);
    auto kids = host.child_elements(list);
    ASSERT_EQ(kids.size(), 2u);
    EXPECT_EQ(host.get_text_content(kids[0]), "item0");
    EXPECT_EQ(host.get_text_content(kids[1]), "item2");
    EXPECT_TRUE(host.consume_mutations());
}

TEST(ScriptEngineTest, WrapperIdentityIsStablePerNode) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto child = Hummingbird::DOM::Element::create(arena, "span");
    child->set_attribute("id", "x");
    child->append_child(Hummingbird::DOM::Text::create(arena, "hi"));
    root->append_child(std::move(child));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // The same DOM node must yield the same JS object across lookups/traversal.
    auto result = engine->eval(
        "var a = document.getElementById('x');"
        "var b = a.parentNode.firstChild;"
        "var same = (a === b) && (a.parentNode === document.getElementById('x').parentNode);"
        "if (!same) throw new Error('wrapper identity broken');"
        "same;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}

TEST(ScriptEngineTest, ResetBindingsNeutralizesStaleWrappers) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto child = Hummingbird::DOM::Element::create(arena, "span");
    child->set_attribute("id", "x");
    root->append_child(std::move(child));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // Stash a wrapper in a global, then simulate navigation teardown.
    ASSERT_TRUE(engine->eval("globalThis.saved = document.getElementById('x');", "inline").ok);
    engine->reset_bindings();

    // The stale wrapper must not dereference the (now invalid) node: its opaque
    // handle was cleared, so accessors read as null rather than crashing.
    auto result = engine->eval(
        "var t = (globalThis.saved.parentNode === null) && (globalThis.saved.nodeType === 0);"
        "if (!t) throw new Error('stale wrapper not neutralized');"
        "t;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}
