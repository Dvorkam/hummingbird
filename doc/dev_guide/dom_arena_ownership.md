# DOM Arena Ownership (JS ⇄ native boundary)

How arena-backed DOM nodes, their JS wrappers, event listeners, and timers are
owned once JavaScript can create, move, and remove them (Milestone 7). This is
the ownership contract completed by story 7.5.4; it captures the rules enforced
by code so new bindings (and M8+ work) don't accidentally break them. Every
binding that retains a JS reference to per-document state must release it in
`reset_bindings()` — see [Navigation teardown](#navigation-teardown).

## The one-owner rule

Every live DOM node is owned in **exactly one** place:

1. its parent's `m_children` vector (`Core::ArenaPtr<Node>`), or
2. the host's **detached set** (`DocumentScriptHost::detached_`), for nodes that
   script created but has not attached, or removed from the tree.

Ownership is an `ArenaPtr<Node>` (a `unique_ptr` with `ArenaDeleter`). Moving a
node between the two places moves the `ArenaPtr`; it is never copied and never
double-owned.

## Arena semantics: detach, never free

The DOM lives in a per-document `ArenaAllocator`. Individual nodes are **never
freed** — the arena is only ever reset wholesale on navigation. So:

- Removing a node from the tree **detaches** it (parent link cleared, `ArenaPtr`
  moved into `detached_`). Its storage stays valid, so JS can re-insert the same
  node object later (`RemoveThenReinsertKeepsNodeValid`) — including in a *later*
  event dispatch: rebinding the host to the same document preserves `detached_`
  (`DetachedNodesSurviveRebindToSameDocument`); only an actual document change
  drops it.
- `ArenaDeleter` runs the destructor (via `std::destroy_at`) but returns no
  memory; `Node`'s virtual destructor makes destroying through `ArenaPtr<Node>`
  correct for `Element`/`Text` subclasses.
- A node created by `document.createElement` and never attached lives in
  `detached_` until the document tears down. That is a bounded, known leak — the
  arena reset reclaims it — which is acceptable per the M7 non-goals.
- Replacing a subtree wholesale — `innerHTML = ...` or `textContent = ...` —
  **detaches** the old children into `detached_` too, it does not destroy them.
  This matters because either can run *inside* an event dispatch (a `change`
  handler doing `list.innerHTML = ''` also clears the node the event is being
  dispatched to). Destroying mid-dispatch would leave the in-flight dispatch and
  the not-yet-rebuilt render tree holding dangling pointers; detaching keeps every
  node valid until the next navigation (`InnerHtmlInChangeHandlerDoesNotCorruptDispatch`).

Tree surgery goes through the `Node` primitives (`append_child_node`,
`insert_child_before`, `remove_child_node`) which keep `m_parent` consistent, and
through `DocumentScriptHost`, which owns `detached_` and enforces the guards
below. The platform (QuickJS) adapter never touches DOM internals — it holds
opaque `Node*` handles and calls `IScriptHost`.

### Guards enforced in the host

- A node cannot become its own descendant (`is_inclusive_ancestor_of`).
- Only elements host children; text/other nodes are leaves.
- `insertBefore`/`replaceChild` require the reference/old node to be a current
  child of the parent.
- Re-parenting removes the node from its previous parent first (DOM move
  semantics).

## Wrapper identity

The JS engine caches **one wrapper object per node** for the document's lifetime
(`QuickJSScriptEngine::node_wrappers_`), so `a.firstChild === a.firstChild` holds
and node-keyed `Set`/`Map` patterns work (`WrapperIdentityIsStablePerNode`). The
cache holds one owning JS reference per node; it is dropped in `reset_bindings()`.

`classList` and `dataset` return small wrapper objects (`DOMTokenList` /
`DOMStringMap`) whose opaque is the raw owner `Node*`. They are created fresh per
access and are **not** in `node_wrappers_`, so — unlike node wrappers — they are
not neutralized on navigation. This is safe for their normal transient use
(`el.classList.add(...)`). Stashing one in a global and using it *after*
navigation would read a dangling `Node*`; that is the same class of gap as JS
globals persisting (see below) and is covered by `T-JS-GLOBAL-ISOLATION-1`. Do
not add APIs that hand long-lived `Node*`-backed wrappers to script without
registering them for neutralization in `reset_bindings()`.

## Navigation teardown

On navigation, `DocumentScriptController::clear()` calls
`IScriptEngine::reset_bindings()` **before** the document arena is reset. That:

1. frees every registered event listener's callback (`listeners_`, 7.2.1) so no
   handler outlives the document (`ListenersTornDownOnNavigation`),
2. frees every pending timer's retained callback + args (`timers_`, 7.3.1) so a
   `setTimeout`/`setInterval` scheduled by the old page can never fire against the
   new one (`TimersAreCanceledOnNavigationTeardown`),
3. neutralizes every cached wrapper (`JS_SetOpaque(..., nullptr)`) so any wrapper
   a script stashed in a global reads as an empty node instead of dereferencing a
   pointer into freed arena storage (`ResetBindingsNeutralizesStaleWrappers`), and
4. releases the cache's references and clears `detached_`.

`NavigationTeardownReleasesPerDocumentState` exercises all of these in one flow.

The event listener registry (`listeners_`) is keyed by the raw arena `Node*`;
each entry owns a reference to its JS callback. Listeners live for the document's
lifetime (removing a node from the tree does not drop its listeners — matching the
DOM, where a re-inserted node keeps them); the whole registry is swept on
navigation. Timers (`timers_`) are owned the same way — each holds a retained
callback (and any trailing args) and is dropped either by `clearTimeout`/
`clearInterval`, by firing once (`setTimeout`), or by the navigation sweep.

## What does NOT reset: the JS global object (known gap)

`reset_bindings()` clears every *per-document* reference above, but the QuickJS
`JSContext` — and therefore the global object — **persists for the tab's
lifetime**. A property a script sets on the global (`globalThis.x = 1`,
`var y = ...` at top level) is still visible after navigating to another page in
the same tab. This is a **deliberate, bounded** shortcut for M7:

- It cannot cause a use-after-free: any DOM wrapper reachable from a stale global
  was neutralized in step 3, so it reads as an empty node, not freed memory.
- The blast radius is name collisions between pages in one tab, which the proof
  targets do not rely on.

Full per-document global isolation (a fresh global per navigation, e.g. by
recreating the context or clearing script-added globals) is deferred to M8 and
tracked as `T-JS-GLOBAL-ISOLATION-1` in `doc/TODOs.md`. When it lands,
`NavigationTeardownReleasesPerDocumentState` — which currently *documents* the
persistence by asserting a stashed global survives — flips to assert it is gone.
