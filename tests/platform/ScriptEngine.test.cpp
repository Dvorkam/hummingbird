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

TEST(ScriptEngineTest, ClassListDatasetAndAttributesThroughBindings) {
    Hummingbird::Core::ArenaAllocator arena(8192, 8);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto item = Hummingbird::DOM::Element::create(arena, "li");
    item->set_attribute("id", "item");
    item->set_attribute("class", "todo");
    root->append_child(std::move(item));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "var el = document.getElementById('item');"
        "var had = el.classList.contains('todo');"
        "el.classList.add('active');"
        "el.classList.toggle('completed');"
        "el.classList.remove('todo');"
        "el.dataset.id = '7';"
        "el.dataset.userName = 'kate';"
        "el.setAttribute('title', 'hi');"
        "var readback = el.dataset.id + '/' + el.getAttribute('title') + '/' + el.className +"
        "  '/' + had + '/' + el.classList.contains('completed');"
        "el.removeAttribute('title');"
        "readback;",
        "inline");
    ASSERT_TRUE(result.ok) << result.error;

    auto* el = host.get_element_by_id("item");
    ASSERT_NE(el, nullptr);
    // classList mutations rewrote the class attribute (todo removed, others added).
    EXPECT_EQ(host.get_attribute(el, "class"), "active completed");
    // dataset round-tripped through data-* attributes with camelCase mapping.
    EXPECT_EQ(host.get_attribute(el, "data-id"), "7");
    EXPECT_EQ(host.get_attribute(el, "data-user-name"), "kate");
    // removeAttribute took effect.
    EXPECT_FALSE(host.has_attribute(el, "title"));
    EXPECT_TRUE(host.consume_mutations());
}

TEST(ScriptEngineTest, FormControlSurfaceThroughBindings) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "form");
    auto field = Hummingbird::DOM::Element::create(arena, "input");
    field->set_attribute("id", "field");
    root->append_child(std::move(field));
    auto box = Hummingbird::DOM::Element::create(arena, "input");
    box->set_attribute("id", "box");
    box->set_attribute("type", "checkbox");
    root->append_child(std::move(box));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "var f = document.getElementById('field');"
        "check('empty', f.value === '');"
        "f.value = 'typed';"
        "check('value', f.value === 'typed');"
        "var b = document.getElementById('box');"
        "check('unchecked', b.checked === false);"
        "b.checked = true;"
        "check('checked', b.checked === true);"
        "f.disabled = true;"
        "check('disabled', f.disabled === true);"
        "f.focus();"  // no-throw smoke
        "f.blur();"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;

    EXPECT_EQ(host.get_value(host.get_element_by_id("field")), "typed");
    EXPECT_TRUE(host.get_checked(host.get_element_by_id("box")));
}

TEST(ScriptEngineTest, InnerHtmlThroughBindings) {
    Hummingbird::Core::ArenaAllocator arena(8192, 8);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto ul = Hummingbird::DOM::Element::create(arena, "ul");
    ul->set_attribute("id", "list");
    root->append_child(std::move(ul));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "var l = document.getElementById('list');"
        "l.innerHTML = '<li>a</li><li class=\"done\">b</li>';"
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "check('count', l.children.length === 2);"
        "check('class', l.children[1].className === 'done');"
        "check('query', l.querySelector('.done').textContent === 'b');"
        "check('serialize', l.innerHTML.indexOf('<li class=\"done\">b</li>') >= 0);"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;

    auto* list = host.get_element_by_id("list");
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(host.child_elements(list).size(), 2u);
    EXPECT_TRUE(host.consume_mutations());
}

TEST(ScriptEngineTest, SelectorQueriesThroughBindings) {
    Hummingbird::Core::ArenaAllocator arena(8192, 8);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto ul = Hummingbird::DOM::Element::create(arena, "ul");
    ul->set_attribute("id", "list");
    for (int i = 0; i < 3; ++i) {
        auto li = Hummingbird::DOM::Element::create(arena, "li");
        li->set_attribute("class", i == 1 ? "item active" : "item");
        li->append_child(Hummingbird::DOM::Text::create(arena, i == 1 ? "mid" : "x"));
        ul->append_child(std::move(li));
    }
    root->append_child(std::move(ul));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "var list = document.getElementById('list');"
        "function check(name, cond) { if (!cond) throw new Error('failed: ' + name); }"
        "check('qsa count', document.querySelectorAll('li.item').length === 3);"
        "var active = document.querySelector('.active');"
        "check('qs text', active.textContent === 'mid');"
        "check('byClass', list.getElementsByClassName('item').length === 3);"
        "check('byTag', document.getElementsByTagName('li').length === 3);"
        "check('scoped', list.querySelectorAll('li').length === 3);"
        "check('matches', active.matches('li.active'));"
        "check('closest', active.closest('#list') === list);"
        "check('identity', active === document.querySelector('.active'));"
        "check('miss', document.querySelector('.nope') === null);"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}

TEST(ScriptEngineTest, EventListenerAddRemoveDedupeAndDispatch) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto el = Hummingbird::DOM::Element::create(arena, "button");
    el->set_attribute("id", "x");
    root->append_child(std::move(el));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "var el = document.getElementById('x');"
        "globalThis.c = 0;"
        "function h() { globalThis.c++; }"
        // Duplicate (type, callback, capture) registration is ignored -> fires once.
        "el.addEventListener('t', h);"
        "el.addEventListener('t', h);"
        "el.dispatchEvent('t');"
        "check('dedupe', globalThis.c === 1);"
        // removeEventListener stops it.
        "el.removeEventListener('t', h);"
        "el.dispatchEvent('t');"
        "check('removed', globalThis.c === 1);"
        // Capture flag is part of the listener identity.
        "globalThis.cc = 0;"
        "function hc() { globalThis.cc++; }"
        "el.addEventListener('c', hc, true);"
        "el.removeEventListener('c', hc);"  // capture mismatch: not removed
        "el.dispatchEvent('c');"
        "check('capture-keep', globalThis.cc === 1);"
        "el.removeEventListener('c', hc, true);"  // now removed
        "el.dispatchEvent('c');"
        "check('capture-remove', globalThis.cc === 1);"
        // The handler sees the event fields and correct this/target.
        "var seen = null;"
        "el.addEventListener('e', function(evt) {"
        "  seen = (evt.type === 'e') && (evt.target === el) && (evt.currentTarget === el) && (this === el);"
        "});"
        "el.dispatchEvent('e');"
        "check('event-fields', seen === true);"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}

TEST(ScriptEngineTest, EventObjectPreventDefaultAndStopImmediate) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto el = Hummingbird::DOM::Element::create(arena, "button");
    el->set_attribute("id", "x");
    root->append_child(std::move(el));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "var el = document.getElementById('x');"
        // dispatchEvent returns true when nothing cancels.
        "el.addEventListener('a', function(e) {});"
        "check('not-canceled', el.dispatchEvent('a') === true);"
        // preventDefault -> defaultPrevented + dispatchEvent returns false.
        "el.addEventListener('b', function(e) {"
        "  check('type', e.type === 'b');"
        "  check('target', e.target === el);"
        "  check('currentTarget', e.currentTarget === el);"
        "  check('not-prevented-yet', e.defaultPrevented === false);"
        "  e.preventDefault();"
        "  check('prevented', e.defaultPrevented === true);"
        "});"
        "check('canceled', el.dispatchEvent('b') === false);"
        // stopImmediatePropagation halts the remaining listeners on this node.
        "globalThis.n = 0;"
        "el.addEventListener('c', function(e) { globalThis.n++; e.stopImmediatePropagation(); });"
        "el.addEventListener('c', function(e) { globalThis.n++; });"
        "el.dispatchEvent('c');"
        "check('immediate-stop', globalThis.n === 1);"
        // Keyboard fields flow from the init object (ahead of 7.2.4 routing).
        "var seenKey = null;"
        "el.addEventListener('keydown', function(e) { seenKey = e.key + '/' + e.code; });"
        "el.dispatchEvent({ type: 'keydown', key: 'Enter', code: 'Enter' });"
        "check('key', seenKey === 'Enter/Enter');"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}

TEST(ScriptEngineTest, CaptureTargetBubblePropagationOrder) {
    // root > gp > p > child
    Hummingbird::Core::ArenaAllocator arena(8192, 8);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto gp = Hummingbird::DOM::Element::create(arena, "div");
    gp->set_attribute("id", "gp");
    auto p = Hummingbird::DOM::Element::create(arena, "div");
    p->set_attribute("id", "p");
    auto child = Hummingbird::DOM::Element::create(arena, "div");
    child->set_attribute("id", "child");
    p->append_child(std::move(child));
    gp->append_child(std::move(p));
    root->append_child(std::move(gp));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "globalThis.log = [];"
        "function on(id, tag, capture) {"
        "  document.getElementById(id).addEventListener('x', function() { globalThis.log.push(tag); }, capture);"
        "}"
        "on('gp', 'gp-cap', true);  on('gp', 'gp-bub', false);"
        "on('p',  'p-cap',  true);  on('p',  'p-bub',  false);"
        "on('child', 'c-cap', true); on('child', 'c-bub', false);"
        // document catches the bubbling event too (delegation).
        "document.addEventListener('x', function(e) {"
        "  globalThis.log.push('doc-bub');"
        "  globalThis.docTarget = (e.target === document.getElementById('child'));"
        "  globalThis.docCurrent = (e.currentTarget === document);"
        "});"
        "document.getElementById('child').dispatchEvent({ type: 'x', bubbles: true });"
        "check('order', globalThis.log.join(',') === 'gp-cap,p-cap,c-cap,c-bub,p-bub,gp-bub,doc-bub');"
        "check('doc-target', globalThis.docTarget === true);"
        "check('doc-current', globalThis.docCurrent === true);"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}

TEST(ScriptEngineTest, StopPropagationAndNonBubblingEvents) {
    Hummingbird::Core::ArenaAllocator arena(8192, 8);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto parent = Hummingbird::DOM::Element::create(arena, "div");
    parent->set_attribute("id", "p");
    auto child = Hummingbird::DOM::Element::create(arena, "div");
    child->set_attribute("id", "c");
    parent->append_child(std::move(child));
    root->append_child(std::move(parent));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "var c = document.getElementById('c');"
        "var p = document.getElementById('p');"
        // stopPropagation in the child's bubble handler prevents the parent's.
        "globalThis.reached = [];"
        "c.addEventListener('e', function(ev) { globalThis.reached.push('c'); ev.stopPropagation(); });"
        "p.addEventListener('e', function(ev) { globalThis.reached.push('p'); });"
        "c.dispatchEvent({ type: 'e', bubbles: true });"
        "check('stopped', globalThis.reached.join(',') === 'c');"
        // A non-bubbling event never reaches the parent's bubble listener.
        "globalThis.reached2 = [];"
        "c.addEventListener('n', function() { globalThis.reached2.push('c'); });"
        "p.addEventListener('n', function() { globalThis.reached2.push('p'); });"
        "c.dispatchEvent('n');"  // bubbles defaults to false
        "check('non-bubbling', globalThis.reached2.join(',') === 'c');"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}

TEST(ScriptEngineTest, LocationHashAndHashchange) {
    Hummingbird::Core::ArenaAllocator arena(1024, 1);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);
    engine->set_location("https://example.dev/todo#/all");

    auto result = engine->eval(
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "check('href', location.href === 'https://example.dev/todo#/all');"
        "check('hash', location.hash === '#/all');"
        "check('window.location', window.location === location);"
        "globalThis.log = [];"
        "window.addEventListener('hashchange', function(e) {"
        "  globalThis.log.push(location.hash + '|' + e.oldURL + '|' + e.newURL);"
        "});"
        // Assigning location.hash fires hashchange, no reload.
        "location.hash = '#/active';"
        "check('href2', location.href === 'https://example.dev/todo#/active');"
        "check('fired', globalThis.log.length === 1);"
        "check('detail', globalThis.log[0] ==="
        "  '#/active|https://example.dev/todo#/all|https://example.dev/todo#/active');"
        // A no-op assignment (same hash) does not re-fire.
        "location.hash = '#/active';"
        "check('no-refire', globalThis.log.length === 1);"
        // Bare 'x' normalizes to '#x'.
        "location.hash = 'plain';"
        "check('normalized', location.hash === '#plain' && globalThis.log.length === 2);"
        "true;",
        "inline");
    ASSERT_TRUE(result.ok) << result.error;

    // navigate_fragment (the app-facing entry) also fires hashchange.
    EXPECT_TRUE(engine->navigate_fragment("https://example.dev/todo#/completed"));
    auto after = engine->eval(
        "if (globalThis.log.length !== 3) throw new Error('nf count ' + globalThis.log.length);"
        "if (location.hash !== '#/completed') throw new Error('nf hash ' + location.hash);"
        "true;",
        "inline");
    EXPECT_TRUE(after.ok) << after.error;

    // Navigating to the same fragment reports no change.
    EXPECT_FALSE(engine->navigate_fragment("https://example.dev/todo#/completed"));
}

TEST(ScriptEngineTest, ListenersTornDownOnNavigation) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto el = Hummingbird::DOM::Element::create(arena, "div");
    el->set_attribute("id", "x");
    root->append_child(std::move(el));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    ASSERT_TRUE(engine
                    ->eval("globalThis.count = 0;"
                           "document.getElementById('x')"
                           "  .addEventListener('ping', function() { globalThis.count++; });"
                           "document.getElementById('x').dispatchEvent('ping');"
                           "if (globalThis.count !== 1) throw new Error('baseline ' + globalThis.count);",
                           "inline")
                    .ok);

    // Simulate navigation teardown: the registry is swept.
    engine->reset_bindings();

    auto after = engine->eval(
        "document.getElementById('x').dispatchEvent('ping');"
        "if (globalThis.count !== 1) throw new Error('stale listener fired: ' + globalThis.count);"
        "true;",
        "inline");
    EXPECT_TRUE(after.ok) << after.error;
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
