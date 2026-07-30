#include <gtest/gtest.h>

#include <string>
#include <vector>

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
    el->set_attribute("data-count", "0");
    Hummingbird::DOM::Element* el_ptr = el.get();
    root->append_child(std::move(el));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // Count into a DOM attribute, not a global: since 9.0.2 the JS global is
    // replaced on navigation, so only the DOM is observable on both sides.
    ASSERT_TRUE(engine
                    ->eval("document.getElementById('x').addEventListener('ping', function() {"
                           "  var e = document.getElementById('x');"
                           "  e.setAttribute('data-count', String(Number(e.getAttribute('data-count')) + 1)); });"
                           "document.getElementById('x').dispatchEvent('ping');",
                           "inline")
                    .ok);
    ASSERT_EQ(host.get_attribute(el_ptr, "data-count"), "1");  // baseline

    // Simulate navigation teardown: the registry is swept.
    engine->reset_bindings();

    auto after = engine->eval("document.getElementById('x').dispatchEvent('ping');", "inline");
    EXPECT_TRUE(after.ok) << after.error;
    EXPECT_EQ(host.get_attribute(el_ptr, "data-count"), "1");  // no stale listener fired
}

// One consolidated teardown/leak check for the JS<->native ownership contract
// (doc/dev_guide/dom_arena_ownership.md, story 7.5.4): a single navigation must
// release every per-document JS reference at once — listeners, timers, node
// wrappers, and (since story 9.0.2) the JS global object itself.
TEST(ScriptEngineTest, NavigationTeardownReleasesPerDocumentState) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto el = Hummingbird::DOM::Element::create(arena, "button");
    el->set_attribute("id", "x");
    el->set_attribute("data-clicks", "0");
    el->set_attribute("data-timers", "0");
    Hummingbird::DOM::Element* el_ptr = el.get();
    root->append_child(std::move(el));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // Page A registers a listener, schedules a repeating timer, stashes a node
    // wrapper in a global, and sets a plain global marker. The listener and the
    // timer count into DOM attributes, not globals: the DOM outlives the JS
    // global, so their post-teardown checks stay observable either way.
    ASSERT_TRUE(engine
                    ->eval("function bump(name) { var e = document.getElementById('x');"
                           "  e.setAttribute(name, String(Number(e.getAttribute(name)) + 1)); }"
                           "globalThis.marker = 'page-a';"
                           "var el = document.getElementById('x');"
                           "el.addEventListener('ping', function() { bump('data-clicks'); });"
                           "setInterval(function() { bump('data-timers'); }, 10);"
                           "globalThis.saved = el;"
                           "el.dispatchEvent('ping');",
                           "inline")
                    .ok);
    EXPECT_EQ(host.get_attribute(el_ptr, "data-clicks"), "1");  // baseline: the listener works
    EXPECT_TRUE(engine->has_pending_timers());

    // --- navigation teardown (before the arena would be reset) ---
    engine->reset_bindings();

    // Listener registry swept: re-dispatching fires nothing.
    ASSERT_TRUE(engine->eval("document.getElementById('x').dispatchEvent('ping');", "inline").ok);
    EXPECT_EQ(host.get_attribute(el_ptr, "data-clicks"), "1");

    // Timers swept: none pending, and advancing the clock fires nothing.
    EXPECT_FALSE(engine->has_pending_timers());
    EXPECT_FALSE(engine->run_due_timers(1000.0));
    EXPECT_EQ(host.get_attribute(el_ptr, "data-timers"), "0");

    // The global object itself is per-document (9.0.2): page A's marker is gone,
    // and so is the global that stashed a node wrapper — the wrapper is no longer
    // reachable from script at all, which is why it can never be dereferenced
    // after the arena resets.
    ASSERT_TRUE(engine
                    ->eval("if (globalThis.marker !== undefined)"
                           "  throw new Error('stale global survived: ' + globalThis.marker);"
                           "if (globalThis.saved !== undefined)"
                           "  throw new Error('stale wrapper still reachable');",
                           "inline")
                    .ok);

    // ...and the fresh global came up with the full binding surface reinstalled.
    ASSERT_TRUE(engine
                    ->eval("if (typeof document.getElementById !== 'function')"
                           "  throw new Error('document bindings missing');"
                           "if (typeof window.setTimeout !== 'function' || typeof setTimeout !== 'function')"
                           "  throw new Error('window bindings missing');"
                           "if (document.getElementById('x').getAttribute('id') !== 'x')"
                           "  throw new Error('node access broken after context swap');",
                           "inline")
                    .ok);
}

// The acceptance case for T-JS-GLOBAL-ISOLATION-1 (story 9.0.2): two documents in
// the same tab share an engine, and page A must not be able to reach page B —
// neither through a plain global, nor by monkey-patching a binding it inherited.
TEST(ScriptEngineTest, NavigationGivesTheNextDocumentAFreshGlobal) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto el = Hummingbird::DOM::Element::create(arena, "span");
    el->set_attribute("id", "x");
    root->append_child(std::move(el));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // Page A leaves behind a global, a global function, and a clobbered binding.
    ASSERT_TRUE(engine
                    ->eval("globalThis.marker = 'page-a';"
                           "function pageAHelper() { return 'a'; }"
                           "document.getElementById = function() { return null; };",
                           "inline")
                    .ok);

    engine->reset_bindings();  // navigation

    // Page B sees none of it, and gets a working document binding back.
    auto page_b = engine->eval(
        "if (globalThis.marker !== undefined) throw new Error('marker leaked: ' + globalThis.marker);"
        "if (typeof pageAHelper !== 'undefined') throw new Error('function declaration leaked');"
        "var found = document.getElementById('x');"
        "if (!found || found.getAttribute('id') !== 'x') throw new Error('clobbered binding leaked');",
        "inline");
    EXPECT_TRUE(page_b.ok) << page_b.error;
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

// Navigation drops the wrapper cache and rebuilds it against the new context.
// Since 9.0.2 replaced the JS global too, a wrapper stashed by the previous page
// is no longer reachable from script at all — the opaque-handle neutralization
// in release_document_state() stays as defense in depth, but the property a page
// can actually observe is that the next document gets fresh, working wrappers
// over the same nodes.
TEST(ScriptEngineTest, ResetBindingsRebuildsWrappersForTheNextDocument) {
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

    auto result = engine->eval(
        "if (globalThis.saved !== undefined) throw new Error('stale wrapper still reachable');"
        "var fresh = document.getElementById('x');"
        "if (fresh.nodeType !== 1 || fresh.parentNode === null)"
        "  throw new Error('rebuilt wrapper is not live');"
        "if (fresh !== document.getElementById('x'))"
        "  throw new Error('wrapper identity not re-established');"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}

namespace {
// Builds <div><div id=out data-log=""></div></div> bound to a fresh engine, so
// timer callbacks can append to `out`'s data-log and the test reads it from C++.
struct TimerFixture {
    Hummingbird::Core::ArenaAllocator arena{8192, 8};
    Hummingbird::Engine::DocumentScriptHost host;
    Hummingbird::ScriptEnginePtr engine;
    // `root` must outlive the fixture: ArenaPtr destruction runs the DOM tree's
    // destructors, so caching it here keeps `out` and the bound host valid.
    Hummingbird::Core::ArenaPtr<Hummingbird::DOM::Element> root;
    Hummingbird::DOM::Element* out = nullptr;

    TimerFixture() {
        root = Hummingbird::DOM::Element::create(arena, "div");
        auto out_el = Hummingbird::DOM::Element::create(arena, "div");
        out_el->set_attribute("id", "out");
        out_el->set_attribute("data-log", "");
        out = out_el.get();
        root->append_child(std::move(out_el));
        host.reset(root.get(), &arena);
        engine = Hummingbird::create_script_engine();
        engine->bind_host(&host);
    }
    std::string log() { return host.get_attribute(out, "data-log"); }
};
}  // namespace

TEST(ScriptEngineTest, TimersFireInDeadlineThenRegistrationOrder) {
    TimerFixture fx;
    // Schedule out of deadline order; two share a deadline (registration breaks the tie).
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "setTimeout(function(){log('B');}, 20);"
        "setTimeout(function(){log('A');}, 10);"
        "setTimeout(function(){log('C');}, 20);",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_TRUE(fx.engine->has_pending_timers());

    EXPECT_FALSE(fx.engine->run_due_timers(5));  // nothing due yet
    EXPECT_EQ(fx.log(), "");
    EXPECT_TRUE(fx.engine->run_due_timers(10));  // A (deadline 10)
    EXPECT_EQ(fx.log(), "A");
    EXPECT_TRUE(fx.engine->run_due_timers(25));  // B then C (deadline 20, registration order)
    EXPECT_EQ(fx.log(), "ABC");
    EXPECT_FALSE(fx.engine->has_pending_timers());
    EXPECT_FALSE(fx.engine->run_due_timers(50));  // one-shots are gone
}

TEST(ScriptEngineTest, ClearTimeoutCancelsBeforeItFires) {
    TimerFixture fx;
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "var id = setTimeout(function(){log('X');}, 10);"
        "clearTimeout(id);",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_FALSE(fx.engine->has_pending_timers());
    EXPECT_FALSE(fx.engine->run_due_timers(100));
    EXPECT_EQ(fx.log(), "");
}

TEST(ScriptEngineTest, SetIntervalRepeatsUntilCleared) {
    TimerFixture fx;
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "var id = setInterval(function(){log('t');}, 10);"
        "setTimeout(function(){clearInterval(id);}, 35);",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;

    EXPECT_TRUE(fx.engine->run_due_timers(10));  // t (reschedules to 20)
    EXPECT_TRUE(fx.engine->run_due_timers(20));  // t (reschedules to 30)
    EXPECT_TRUE(fx.engine->run_due_timers(30));  // t (reschedules to 40)
    EXPECT_EQ(fx.log(), "ttt");
    fx.engine->run_due_timers(35);  // clearInterval fires; interval (next at 40) removed
    EXPECT_FALSE(fx.engine->has_pending_timers());
    EXPECT_FALSE(fx.engine->run_due_timers(100));
    EXPECT_EQ(fx.log(), "ttt");
}

TEST(ScriptEngineTest, TimersAreCanceledOnNavigationTeardown) {
    TimerFixture fx;
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "setTimeout(function(){log('X');}, 10);"
        "setInterval(function(){log('Y');}, 10);",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_TRUE(fx.engine->has_pending_timers());

    // Navigation teardown sweeps the document's timers.
    fx.engine->reset_bindings();
    EXPECT_FALSE(fx.engine->has_pending_timers());
    EXPECT_FALSE(fx.engine->run_due_timers(1000));
    EXPECT_EQ(fx.log(), "");
}

TEST(ScriptEngineTest, TimerCallbackCanScheduleAndClearReentrantly) {
    TimerFixture fx;
    // A one-shot that, when it fires, schedules another one-shot: the new timer
    // must not fire in the same pass (avoids a same-tick storm) but on a later run.
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "setTimeout(function(){ log('1'); setTimeout(function(){ log('2'); }, 0); }, 10);",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;

    EXPECT_TRUE(fx.engine->run_due_timers(10));  // fires '1' and schedules the nested timer
    EXPECT_EQ(fx.log(), "1");                    // '2' did NOT fire in this pass
    EXPECT_TRUE(fx.engine->has_pending_timers());
    EXPECT_TRUE(fx.engine->run_due_timers(10));  // now the nested timer (deadline 10) fires
    EXPECT_EQ(fx.log(), "12");
    EXPECT_FALSE(fx.engine->has_pending_timers());
}

TEST(ScriptEngineTest, MicrotasksDrainAfterScriptTask) {
    TimerFixture fx;
    // Synchronous code runs first; the promise reaction is a microtask that runs
    // at the checkpoint after the script task, before eval returns (7.3.2).
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "log('1');"
        "Promise.resolve().then(function(){ log('3'); });"
        "log('2');",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(fx.log(), "123");  // 1,2 synchronous, then the drained microtask 3
}

TEST(ScriptEngineTest, MicrotaskRunsBeforeNextTimerTask) {
    TimerFixture fx;
    // Interleaving: a microtask queued in the script must run before the next
    // task (the timer callback); a microtask queued inside the timer callback
    // runs before that callback's checkpoint returns.
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "log('s');"
        "Promise.resolve().then(function(){ log('m'); });"
        "setTimeout(function(){ log('t'); Promise.resolve().then(function(){ log('u'); }); }, 0);",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(fx.log(), "sm");  // script 's', then its microtask 'm' — timer not yet due
    EXPECT_TRUE(fx.engine->has_pending_timers());

    EXPECT_TRUE(fx.engine->run_due_timers(0));  // fires 't', then drains its microtask 'u'
    EXPECT_EQ(fx.log(), "smtu");
}

// A host callback can re-enter the engine while script is still on the stack:
// JS element.focus() routes through IScriptHost and comes back as a nested focus
// dispatch. That nested dispatch must NOT run the outer script's queued promise
// continuations — a real event loop reaches a microtask checkpoint when the JS
// stack empties, not when an inner task returns. The outermost entry drains what
// the whole re-entrant chain queued (T-DISPATCH-MICROTASK-REENTRANT-1, 9.0.1).
TEST(ScriptEngineTest, NestedDispatchDefersMicrotaskCheckpointToOutermost) {
    TimerFixture fx;
    auto field_el = Hummingbird::DOM::Element::create(fx.arena, "input");
    field_el->set_attribute("id", "field");
    fx.root->append_child(std::move(field_el));

    // The focus sink is the re-entry point: it dispatches `focus` from inside
    // the click listener's call to focus().
    fx.host.set_focus_sink([&](Hummingbird::DOM::Element* el, bool focused) {
        if (!focused) return;
        fx.engine->dispatch_dom_event(el, Hummingbird::ScriptDomEvent{"focus", false, false, "", ""});
    });

    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "document.getElementById('field').addEventListener('focus', function(){"
        "  log('f'); Promise.resolve().then(function(){ log('n'); }); });"
        "document.getElementById('out').addEventListener('click', function(){"
        "  log('c');"
        "  Promise.resolve().then(function(){ log('m'); });"
        "  document.getElementById('field').focus();"
        "  log('d'); });",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(fx.log(), "");  // nothing runs until the click

    fx.engine->dispatch_dom_event(fx.out, Hummingbird::ScriptDomEvent{"click", true, true, "", ""});

    // 'c' -> nested focus 'f' -> back in the click listener 'd', and only then
    // the checkpoint: 'm' (queued by the outer listener) before 'n' (queued by
    // the nested one), FIFO. Without the guard the nested dispatch's drain would
    // run 'm' — and 'n' — before 'd'.
    EXPECT_EQ(fx.log(), "cfdmn");
}

TEST(ScriptEngineTest, ChainedMicrotasksAllDrainInOrder) {
    TimerFixture fx;
    // A microtask that queues another microtask: the checkpoint keeps draining
    // until the queue is empty, in FIFO order.
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "Promise.resolve().then(function(){ log('a');"
        "  Promise.resolve().then(function(){ log('c'); }); });"
        "Promise.resolve().then(function(){ log('b'); });",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(fx.log(), "abc");  // a,b at depth 1 (FIFO), then c queued by a
}

// Missing-API telemetry (7.5.2): touching an unimplemented API logs once
// (deduped, first-touch order) and no-ops instead of throwing, so the rest of the
// script keeps running — the fail-soft contract that keeps hn.js's fetch/XHR
// references from killing its collapse handlers.
TEST(ScriptEngineTest, MissingApiTelemetryIsFailSoftAndDeduped) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "globalThis.ran = 0;"
        "fetch('/a'); fetch('/b');"                                    // real since 9.1.1: NOT reported
        "matchMedia('(min-width:0)'); matchMedia('(max-width:0)');"    // reported once (still a stub)
        "var x = new XMLHttpRequest(); x.open('GET', '/'); x.send();"  // reported once
        "globalThis.ran = 1;",                                         // proves no abort
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
    ASSERT_TRUE(engine->eval("if (globalThis.ran !== 1) throw new Error('script aborted');", "inline").ok);

    // Deduped, in first-touch order. localStorage and sessionStorage are NOT here:
    // both became real (if inert, with no store) APIs in 8.2.2 / 8.2.3. `fetch`
    // left this list in 9.1.1 for the same reason — and note the calls above do
    // not hang: with no fetch sink wired up, the binding REJECTS rather than
    // returning the never-settling promise the old stub handed back.
    EXPECT_EQ(engine->missing_apis(), (std::vector<std::string>{"matchMedia", "XMLHttpRequest"}));

    // Telemetry is per-document: navigation teardown clears it.
    engine->reset_bindings();
    EXPECT_TRUE(engine->missing_apis().empty());
}

// Minimal URL + URLSearchParams polyfill (needed for Hacker News' hn.js, which
// does `new URL(el.href, location)` in its delegated click handler). It must
// parse absolute + relative URLs, expose pathname/searchParams/hash, and — above
// all — never throw, so the handler reaches its collapse branch.
TEST(ScriptEngineTest, UrlPolyfillParsesAndNeverThrows) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "var base = 'https://news.ycombinator.com/item?id=9';"
        // Absolute pseudo-scheme (the collapse toggle's href) — base ignored, no throw.
        "var j = new URL('javascript:void(0)', base);"
        "check('js-proto', j.protocol === 'javascript:');"
        "check('js-path', j.pathname === 'void(0)');"
        "check('js-not-vote', j.pathname !== '/vote');"
        // Relative resolved against the base document.
        "var v = new URL('vote?id=9&how=up&goto=news', base);"
        "check('vote-path', v.pathname === '/vote');"
        "check('vote-id', v.searchParams.get('id') === '9');"
        "check('vote-how', v.searchParams.get('how') === 'up');"
        "check('vote-missing', v.searchParams.get('nope') === null);"
        // Absolute http with query + fragment.
        "var f = new URL('https://x.test/a/b?k=1#frag');"
        "check('abs-path', f.pathname === '/a/b');"
        "check('abs-hash', f.hash === '#frag');"
        "check('abs-host', f.host === 'x.test');"
        // Garbage must not throw — opaque pathname, empty params.
        "var g = new URL('%%% not a url @@@', base);"
        "check('garbage-params', g.searchParams.get('x') === null);"
        "check('URLSearchParams', new URLSearchParams('a=1&b=2').get('b') === '2');"
        "true;",
        "inline");
    EXPECT_TRUE(result.ok) << result.error;
}

TEST(ScriptEngineTest, RequestAnimationFrameFiresOncePerFrameWithTimestamp) {
    TimerFixture fx;
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "requestAnimationFrame(function(ts){ log(ts >= 16 ? 'A' : 'F'); });",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_TRUE(fx.engine->has_pending_animation_frames());

    EXPECT_TRUE(fx.engine->run_animation_frames(16.0));       // fires with the frame timestamp
    EXPECT_EQ(fx.log(), "A");                                 // ts was passed (>= 16)
    EXPECT_FALSE(fx.engine->has_pending_animation_frames());  // one-shot
    EXPECT_FALSE(fx.engine->run_animation_frames(32.0));      // nothing re-registered
    EXPECT_EQ(fx.log(), "A");
}

TEST(ScriptEngineTest, RafReRequestAnimatesWithoutQueueGrowth) {
    TimerFixture fx;
    // A callback that re-requests rAF: it must run once per frame and never let
    // the queue grow (the "animate without queue growth" contract).
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "function frame(ts){ log('x'); requestAnimationFrame(frame); }"
        "requestAnimationFrame(frame);",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(fx.engine->has_pending_animation_frames());
        EXPECT_TRUE(fx.engine->run_animation_frames(i * 16.0));  // exactly one callback each frame
    }
    EXPECT_EQ(fx.log(), "xxx");
    EXPECT_TRUE(fx.engine->has_pending_animation_frames());  // still exactly one queued
}

TEST(ScriptEngineTest, CancelAnimationFrameStopsTheCallback) {
    TimerFixture fx;
    auto r = fx.engine->eval(
        "function log(c){var o=document.getElementById('out');"
        "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}"
        "var id = requestAnimationFrame(function(){ log('n'); });"
        "cancelAnimationFrame(id);",
        "inline");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_FALSE(fx.engine->has_pending_animation_frames());
    EXPECT_FALSE(fx.engine->run_animation_frames(16.0));
    EXPECT_EQ(fx.log(), "");
}

TEST(ScriptEngineTest, AnimationFramesCanceledOnNavigationTeardown) {
    TimerFixture fx;
    ASSERT_TRUE(fx.engine
                    ->eval("requestAnimationFrame(function(){});"
                           "requestAnimationFrame(function(){});",
                           "inline")
                    .ok);
    EXPECT_TRUE(fx.engine->has_pending_animation_frames());
    fx.engine->reset_bindings();
    EXPECT_FALSE(fx.engine->has_pending_animation_frames());
    EXPECT_FALSE(fx.engine->run_animation_frames(16.0));
}

// T-JS-MISSING-API-COVERAGE-1: the widened reporting surface. Before this the
// prelude reported four names, two of which (localStorage/sessionStorage) cover
// features implemented in 8.2.2/8.2.3 and so never fire — leaving exactly two
// observable APIs to derive M12's scope from.
//
// The critical property is not that each reports, it is that each is USED the
// way a page uses it and still does not throw. A stub that reports and then
// dies on the next line is worse than no stub: the page fails anyway, and the
// telemetry claims it was handled.
TEST(ScriptEngineTest, WidenedFailSoftStubsReportAndSurviveRealisticUse) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    const auto result = engine->eval(
        "globalThis.ran = 0;"
        // Observers: construct, then call the methods a page calls on them.
        "var io = new IntersectionObserver(function () {});"
        "io.observe({}); io.unobserve({}); io.takeRecords(); io.disconnect();"
        "var mo = new MutationObserver(function () {}); mo.observe({}, {}); mo.disconnect();"
        "var ro = new ResizeObserver(function () {}); ro.observe({}); ro.disconnect();"
        // Custom elements: define + get, the two a page actually reaches for.
        "customElements.define('x-thing', function () {}); customElements.get('x-thing');"
        // WebSocket: construct and use, including the state a page branches on.
        "var ws = new WebSocket('wss://example.test/s'); ws.send('hi'); ws.close();"
        "globalThis.wsClosed = (ws.readyState === WebSocket.CLOSED);"
        "getComputedStyle({}).getPropertyValue('width');"
        "globalThis.ua = navigator.userAgent;"
        "alert('x'); globalThis.confirmed = confirm('y'); globalThis.prompted = prompt('z');"
        "globalThis.cloned = structuredClone({ a: [1, 2] }).a[1];"
        "globalThis.ran = 1;",
        "inline");
    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_TRUE(engine->eval("if (globalThis.ran !== 1) throw new Error('script aborted');", "inline").ok);

    // The values pages branch on must be honest, not merely present.
    EXPECT_TRUE(engine->eval("if (!globalThis.wsClosed) throw new Error('socket claimed to be open');", "inline").ok)
        << "a stub socket must report CLOSED, never pretend a connection exists";
    EXPECT_TRUE(engine->eval("if (globalThis.ua !== '') throw new Error('fabricated a user agent');", "inline").ok)
        << "navigator.userAgent stays empty: M8 owns identity, and a second answer here would contradict it";
    EXPECT_TRUE(engine->eval("if (globalThis.confirmed !== false) throw new Error('confirm said yes');", "inline").ok)
        << "no dialog surface exists, so the user cannot have agreed to anything";
    EXPECT_TRUE(engine->eval("if (globalThis.prompted !== null) throw new Error('prompt invented input');", "inline").ok);
    EXPECT_TRUE(engine->eval("if (globalThis.cloned !== 2) throw new Error('structuredClone lost data');", "inline").ok)
        << "a JSON-shaped clone must actually deep-copy";

    const auto reported = engine->missing_apis();
    const auto reported_has = [&](const char* name) {
        return std::find(reported.begin(), reported.end(), name) != reported.end();
    };
    for (const char* name : {"IntersectionObserver", "MutationObserver", "ResizeObserver", "customElements",
                             "WebSocket", "getComputedStyle", "navigator.userAgent", "alert", "confirm", "prompt",
                             "structuredClone"}) {
        EXPECT_TRUE(reported_has(name)) << name << " was used but never reported";
    }
    // Deduped per document even though customElements was touched twice.
    EXPECT_EQ(std::count(reported.begin(), reported.end(), std::string("customElements")), 1);
}

// requestIdleCallback is the one stub that must actually RUN its callback.
// Pages defer real initialization into it; dropping the callback leaves the
// page half-built with no error to explain why.
TEST(ScriptEngineTest, RequestIdleCallbackStubStillRunsTheCallback) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    ASSERT_TRUE(engine->eval("globalThis.idle = 0;"
                             "requestIdleCallback(function (deadline) {"
                             "  globalThis.idle = deadline.timeRemaining() === 0 ? 2 : 1;"
                             "});",
                             "inline")
                    .ok);
    EXPECT_TRUE(engine->eval("if (globalThis.idle !== 0) throw new Error('ran synchronously');", "inline").ok)
        << "an idle callback must not run inside the call that scheduled it";

    EXPECT_TRUE(engine->run_due_timers(1000.0));
    EXPECT_TRUE(engine->eval("if (globalThis.idle !== 2) throw new Error('callback never ran');", "inline").ok);
}

// A real implementation landing later must win over its stub. The `typeof`
// guard is what makes that true, and it is the reason a stub can be added
// without scheduling its own removal.
TEST(ScriptEngineTest, FailSoftStubsNeverOverwriteARealImplementation) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // `fetch` is real since 9.1.1 and installed before the prelude runs, so it
    // is the live proof that the guard holds rather than a synthetic one.
    ASSERT_TRUE(engine->eval("globalThis.kind = typeof fetch;", "inline").ok);
    EXPECT_TRUE(engine->eval("if (globalThis.kind !== 'function') throw new Error('fetch missing');", "inline").ok);
    EXPECT_TRUE(engine->missing_apis().empty()) << "a real API must not be reported as missing";
}

// Story 9.5.2 follow-up, driven by a real log rather than a guess. A live sweep
// over wikipedia.org / hn.algolia.com produced only TWO `[missing-api]` lines
// from the 14-name stub list, while the same run showed scripts dying on
// `ReferenceError: Element is not defined`. The stub list can only report gaps
// somebody predicted; the ReferenceErrors name the ones nobody did.
TEST(ScriptEngineTest, ReferenceErrorsAreHarvestedAsMissingApis) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // The exact failure wikipedia.org's bundle hit. `Element` is a DOM interface
    // object this engine does not expose, and it was NOT on the stub list.
    EXPECT_FALSE(engine->eval("Element.prototype.matches = function () {};", "index.js").ok);

    const auto reported = engine->missing_apis();
    ASSERT_EQ(reported.size(), 1u);
    // The suffix matters: a stub hit means the page carried on without the
    // feature, this means the script DIED and nothing after it ran. A triage
    // that merged the two would rank a fatal gap alongside a handled one.
    EXPECT_EQ(reported[0], "Element (ReferenceError)");
}

// Minified bundles throw on their own mangled locals. `aa is not defined` was
// in the same live log and says nothing about this engine, so it must not
// pollute the count the next milestone's scope is chosen from.
TEST(ScriptEngineTest, MinifiedLocalsAreNotMistakenForMissingApis) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    EXPECT_FALSE(engine->eval("aa.b = 1;", "inline").ok);
    EXPECT_FALSE(engine->eval("x();", "inline").ok);
    EXPECT_TRUE(engine->missing_apis().empty()) << "short mangled locals are noise, not findings";

    // A non-ReferenceError failure is not a missing API either — the same log
    // had `Error: Invariant failed` from a framework's own assertion.
    EXPECT_FALSE(engine->eval("throw new Error('Invariant failed');", "main.js").ok);
    EXPECT_TRUE(engine->missing_apis().empty());
}

// `navigator` existing is not enough if its shape is wrong: the live sweep
// caught Google Tag Manager doing `.indexOf` on a navigator field and dying on
// `undefined`. The string-typed fields must be strings.
TEST(ScriptEngineTest, NavigatorStubFieldsAreStringsNotUndefined) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    const auto result = engine->eval(
        "globalThis.probed = 0;"
        "var n = navigator;"
        "n.userAgent.indexOf('x'); n.appVersion.indexOf('x'); n.platform.indexOf('x');"
        "n.vendor.indexOf('x'); n.product.indexOf('x'); n.appName.indexOf('x');"
        "n.language.indexOf('x');"
        "globalThis.probed = 1;",
        "inline");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(engine->eval("if (globalThis.probed !== 1) throw new Error('died');", "inline").ok);
}

// T-JS-WINDOW-IS-GLOBAL-1. `window` used to be a separate object with a
// hand-mirrored subset of globals, so anything not on that list was missing from
// it. A browser has one object: window === globalThis.
TEST(ScriptEngineTest, WindowIsTheGlobalObjectSoEveryGlobalIsReachableBothWays) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    const auto result = engine->eval(
        "function check(n, c) { if (!c) throw new Error('failed: ' + n); }"
        "check('self', window === globalThis);"
        "check('window.window', window.window === window);"
        // The ones that were missing, and the reason this story exists. Identity
        // rather than typeof: a mirrored copy would pass a typeof check.
        "check('console', window.console === console);"
        "check('document', window.document === document);"
        "check('location', window.location === location);"
        "check('fetch', window.fetch === fetch);"
        "check('navigator', window.navigator === navigator);"
        "check('matchMedia', window.matchMedia === matchMedia);"
        "check('setTimeout', window.setTimeout === setTimeout);"
        "check('localStorage', window.localStorage === localStorage);"
        // Everything T-JS-MISSING-API-COVERAGE-1 added comes along for free,
        // which is the point: the mirror list can no longer fall behind.
        "check('IntersectionObserver', window.IntersectionObserver === IntersectionObserver);"
        "check('MutationObserver', window.MutationObserver === MutationObserver);"
        "check('customElements', window.customElements === customElements);"
        "check('WebSocket', window.WebSocket === WebSocket);"
        "check('structuredClone', window.structuredClone === structuredClone);"
        "check('URL', window.URL === URL);"
        // A global assigned through `window` is a bare global, and the reverse.
        "window.hbViaWindow = 'w'; check('alias-out', hbViaWindow === 'w');"
        "globalThis.hbViaGlobal = 'g'; check('alias-in', window.hbViaGlobal === 'g');"
        "true;",
        "inline");
    ASSERT_TRUE(result.ok) << result.error;
}

// The exact live failure this fixes: MediaWiki's startup module died with
// "TypeError: cannot read property 'warn' of undefined" on
// /w/load.php?...modules=startup, which is the shape of window.console.warn.
TEST(ScriptEngineTest, WindowConsoleIsUsableLikeAPageExpects) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    const auto result = engine->eval(
        "globalThis.ran = 0;"
        "window.console.warn('from window.console');"
        "window.console.log('and log');"
        "var con = window.console || {};"
        "if (typeof con.warn !== 'function') { throw new Error('no warn'); }"
        "globalThis.ran = 1;",
        "inline");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(engine->eval("if (globalThis.ran !== 1) throw new Error('script aborted');", "inline").ok);
    // And it is not reported as a missing API, because it is really there.
    EXPECT_TRUE(engine->missing_apis().empty());
}

// A bare `addEventListener` is window.addEventListener. It used to be a
// ReferenceError, because the trio lived only on the separate window object.
TEST(ScriptEngineTest, BareAddEventListenerRegistersOnWindow) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);
    engine->set_location("https://example.dev/page#one");

    ASSERT_TRUE(engine
                    ->eval("globalThis.seen = [];"
                           // Bare call — no `window.` prefix.
                           "addEventListener('hashchange', function (e) { globalThis.seen.push(e.newURL); });",
                           "inline")
                    .ok);

    EXPECT_TRUE(engine->navigate_fragment("https://example.dev/page#two"));
    const auto check = engine->eval(
        "if (globalThis.seen.length !== 1) throw new Error('listener never fired');"
        "if (globalThis.seen[0] !== 'https://example.dev/page#two') throw new Error('wrong url');"
        "true;",
        "inline");
    EXPECT_TRUE(check.ok) << check.error;
}

// window is now the isolation surface too: since it IS the global, 9.0.2's
// per-document teardown must wipe it. Worth asserting explicitly — the identity
// change would otherwise be a quiet way to reintroduce cross-document leakage.
TEST(ScriptEngineTest, WindowPropertiesDoNotSurviveIntoTheNextDocument) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    ASSERT_TRUE(engine->eval("window.hbLeak = 'from page A';", "inline").ok);
    ASSERT_TRUE(engine->eval("if (window.hbLeak !== 'from page A') throw new Error('not set');", "inline").ok);

    engine->reset_bindings();  // navigation teardown
    engine->bind_host(&host);

    const auto after = engine->eval(
        "if (typeof window.hbLeak !== 'undefined') throw new Error('leaked: ' + window.hbLeak);"
        "if (window !== globalThis) throw new Error('window lost its self-reference');"
        "true;",
        "inline");
    EXPECT_TRUE(after.ok) << after.error;
}

// `console` had exactly one method. An object existing is not the same as it
// being usable: console.warn -- which MediaWiki's startup module calls -- was
// "not a function", so fixing window.console alone would have moved the same
// page's death one line later.
TEST(ScriptEngineTest, ConsoleExposesTheMethodsPagesActuallyCall) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    // Every method is called the way a page calls it, not merely typeof-checked:
    // a missing one must fail here rather than on somebody's real page.
    const auto result = engine->eval(
        "globalThis.ran = 0;"
        "var names = ['log','info','debug','warn','error','trace','dir','table',"
        "             'group','groupCollapsed','groupEnd','time','timeEnd','timeLog','count','assert'];"
        "for (var i = 0; i < names.length; i++) {"
        "  if (typeof console[names[i]] !== 'function') { throw new Error('missing console.' + names[i]); }"
        "  console[names[i]]('probe ' + names[i]);"
        "}"
        // Multiple arguments and non-strings must not throw either.
        "console.log('a', 1, true, null, undefined, { k: 'v' }, [1, 2]);"
        "console.warn();"
        "globalThis.ran = 1;",
        "inline");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(engine->eval("if (globalThis.ran !== 1) throw new Error('script aborted');", "inline").ok);
    EXPECT_TRUE(engine->missing_apis().empty()) << "console is real, so nothing should be reported missing";
}
