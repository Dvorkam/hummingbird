> **Status: Active** — scope revalidated at kickoff 2026-07-17. Additions from the
> kickoff review: external script loading (7.0.1 — only inline scripts run today),
> real-world secondary proof (HN comment collapse), form-control JS surface +
> checkbox MVP (7.1.5), fragment navigation (7.2.5), and browser-chrome
> conveniences (7.6: back/forward, bookmarks).

## Milestone 7 North Star Deliverable

**Before (after Milestone 6):**

* JS can eval, `getElementById`, set `textContent`, and receive `click`/`load` dispatch
  on directly-targeted nodes — but there is no generic event system (no capture/bubble,
  no `removeEventListener`, no event objects worth the name).
* JS cannot *build* UI: no `createElement`/`appendChild`/`removeChild`, no
  `querySelector`, no `innerHTML`.
* Time does not exist: no `setTimeout`, no microtask pump (Promises resolve but their
  jobs never run), no `requestAnimationFrame`.
* Mutations from JS trigger coarse rebuilds rather than scoped invalidation.

**After (Milestone 7 done):**

* **A real, mutable, scriptable document**: JS builds and restructures the DOM through
  standard primitives with strict arena ownership.
* **Event System v1**: `addEventListener`/`removeEventListener`, real event objects,
  capture → target → bubble propagation, keyboard/input/submit events routed from
  Platform.
* **Scheduling**: task queue (`setTimeout`/`setInterval`), microtask queue integrated
  with the main-loop tick, `requestAnimationFrame`.
* **Deterministic invalidation**: N mutations inside one handler produce one
  style/layout/paint pass.
* **Proof target:** a pinned **vanilla-JS TodoMVC snapshot** is fully usable — add,
  toggle, edit, filter, clear-completed — in a CI harness.
* **Real-world secondary proof:** on a **Hacker News item page** (pinned snapshot;
  live as a manual check), `hn.js` initializes and **comment collapse/expand ([–]/[+])
  works**. Verified against the live `hn.js` (2026-07-17): the collapse path needs
  exactly M7's surface — `addEventListener` delegation on `document`, event objects
  (`target`, `stopPropagation`, `preventDefault`), `className` manipulation with
  restyle, `getAttribute`, `nextElementSibling` traversal, `getElementsByClassName` —
  plus fail-soft stubs for the APIs we don't do yet (`fetch`/XHR for voting → M8/M9,
  `scrollIntoView` → no-op). Voting/login staying broken is expected and fine; it is
  M8's proof target.

---

## Non-Goals (keep the blast radius controlled)

* No fetch/XHR or any networking JS API (M9). No cookies/storage bindings (M8).
* No History API (`pushState` etc. — M12); the only same-document navigation in scope
  is the fragment/`hashchange` slice (7.2.5). No observer APIs (`MutationObserver`
  etc.), no custom elements (M12).
* No Shadow DOM.
* No exhaustive event-type coverage — an enumerated set (see 7.2.4) is the contract.
* `innerHTML` reuses the existing recovery-oriented parser on a fragment; no separate
  spec-grade fragment parsing algorithm.
* No GC integration work beyond the documented ownership rules (no cycle collector
  heroics; leaks across the JS/native boundary are acceptable if bounded and known).
* No new threading (DOM/layout/style stays main-thread deterministic).

---

## Critical Path (what must land for the North Star)

**Must-have**

* External script loading (7.0.1) — both proof targets ship their JS as separate
  files; carries the T-RESOURCE-TYPE-TABLE-1 paydown (Script is the 5th resource type).
* DOM construction/mutation primitives with arena ownership (7.1.1) + `innerHTML` (7.1.4).
* `querySelector`/`querySelectorAll` over the existing SelectorMatcher (7.1.3).
* Form-control JS surface (7.1.5) — every TodoMVC flow reads/writes `.value` and
  `.checked`.
* EventTarget with propagation and real event objects (7.2.1–7.2.3).
* Keyboard/input/submit event routing (7.2.4) — TodoMVC is keyboard-driven.
* Interactive checkbox control (7.2.6) — TodoMVC's toggle is a checkbox whose
  `change` handler drives the re-render (split from 7.1.5; needs 7.2.1–7.2.4).
* Fragment navigation + `hashchange` (7.2.5) — TodoMVC's filters are hash-routed.
* Task + microtask queues in the main loop (7.3.1–7.3.2).
* Batched invalidation per task (7.4.1).
* TodoMVC snapshot harness in CI (7.5.1).

**Nice-to-have (if schedule allows)**

* `requestAnimationFrame` (7.3.3) — cheap once the frame tick owns the queues.
* `classList`/`dataset` conveniences beyond what TodoMVC touches (7.1.2 minimum scope
  is what the target needs).
* Browser chrome: back/forward (7.6.1) and bookmarks MVP (7.6.2) — dev-convenience,
  independent of the engine-side critical path; good interleave work.

---

## Milestone 7 Done When

* TodoMVC (pinned snapshot) passes a scripted add/toggle/edit/filter/clear flow in the
  headless harness, and CI fails on regression.
* On a pinned Hacker News item-page snapshot, comment collapse/expand works end-to-end
  (secondary proof; live-site check stays a manual gate). Voting/login remain broken
  by design (M8/M9).
* Capture/target/bubble order is correct for nested listeners (event-order tests).
* A handler performing many mutations triggers exactly one style/layout/paint pass.
* `setTimeout(fn, 0)` and Promise jobs interleave in spec order (task vs microtask
  tests).
* Missing-API telemetry and parser fuzzing run in CI (standing guardrails start here).
* JS/native ownership rules are documented (`doc/dev_guide/`), and teardown tests show
  no stale listeners or dangling node references after navigation.

---

## Stories

### 7.0 - Script Loading (prerequisite — both proof targets ship external JS)

* **Story 7.0.1: External Script Loading (T-SCRIPT-SRC-1)**
* **Goal:** fetch and execute `<script src="...">` — TodoMVC's `app.js` and HN's
  `hn.js` are external files; today only inline `<script>` bodies run
  (`DocumentLinkDiscovery` ignores the `src` attribute).
* **Scope:** `ResourceType::Script` through the store/loader/update-processor
  pipeline; execution-order MVP: after parse, scripts run in document order with
  inline and external interleaved (classic-script semantics approximated; `async`/
  `defer` may collapse to that same order), all before `load` dispatch. **Pay down
  T-RESOURCE-TYPE-TABLE-1 here:** Script is the 5th resource type — introduce the
  per-type descriptor table instead of hand-mirroring the Image/Font boilerplate a
  fifth time.
* **Acceptance:** a page whose behavior lives in an external .js file works
  identically to the same script inlined; script order is deterministic.
* **Tests:** resource pipeline tests + script execution-order tests + the
  table-drives-dispatch test from T-RESOURCE-TYPE-TABLE-1.

### 7.1 - DOM Core (JS can build UI)

* **Story 7.1.1: Mutation Primitives With Arena Ownership**
* **Goal:** `createElement`, `createTextNode`, `appendChild`, `insertBefore`,
  `removeChild`, `replaceChild`; read-only traversal accessors (`parentNode`,
  `children`/`childNodes`, `firstChild`/`lastChild`,
  `nextElementSibling`/`previousElementSibling`) — real-page delegation patterns
  (hn.js) walk siblings, so traversal ships with mutation, not later.
* **Scope:** core/dom node factories + tree surgery; document the rule for nodes
  created after initial parse (same arena; removal detaches, never frees).
* **Acceptance:** JS builds a list of elements from scratch; removing/reinserting nodes
  never corrupts the tree or the arena.
* **Tests:** DOM mutation unit tests + invalidation integration tests.

* **Story 7.1.2: Attributes, classList, dataset**
* **Goal:** `getAttribute`/`setAttribute`/`removeAttribute`, `classList`
  add/remove/toggle/contains, `dataset` read/write.
* **Scope:** core/dom + JS bindings; class changes feed selector re-match.
* **Acceptance:** toggling a class visibly restyles; `dataset.id` round-trips.
* **Tests:** DOM + style invalidation tests.

* **Story 7.1.3: querySelector / querySelectorAll**
* **Goal:** subtree selector queries reusing SelectorMatcher; plus the cheap
  derivatives once the matcher entry point exists — `Element.matches`,
  `Element.closest`, and legacy `getElementsByClassName`/`getElementsByTagName`
  (hn.js and most pre-framework pages use the legacy forms, not `querySelector`).
* **Scope:** traversal + matcher entry point + JS bindings (static NodeList snapshot).
* **Acceptance:** the selector subset supported by the style engine works identically
  from JS.
* **Tests:** query tests mirroring existing selector-matcher coverage.

* **Story 7.1.4: innerHTML (fragment parse)**
* **Goal:** `element.innerHTML = "<li>…</li>"` replaces children via the existing
  HTML parser in fragment mode.
* **Scope:** parser fragment entry point + subtree teardown/attach; recovery behavior
  matches document parsing.
* **Acceptance:** TodoMVC's template-string rendering path works.
* **Tests:** fragment parse + mutation tests, including malformed input.

* **Story 7.1.5: Form Control JS Surface** — *(the interactive checkbox control
  originally bundled here moved to Story 7.2.6, which depends on the event system;
  see the note there. This story is now the JS-surface half only.)*
* **Goal:** JS read/write of `input.value`, `input.checked`, `disabled`;
  `element.focus()`/`blur()`.
* **Scope:** JS bindings over the existing form-control state. `.value` reflects
  the `value` attribute; `.checked`/`.disabled` reflect boolean-attribute state;
  `focus()`/`blur()` reflect the `:focus` pseudo-state (hooking `focus()` to the
  live text-edit caret target is part of 7.2.6). Radio groups, `<select>`, and
  everything else stay in M11 forms v2.
* **Why now:** every TodoMVC interaction runs through this — add reads+clears
  `.value`, toggle reads/writes `.checked`, edit mode calls `.focus()`. Without
  this the North Star is unreachable no matter how good DOM/events are.
* **Acceptance:** JS reads what the user typed, clears the field, reads/writes
  `.checked` and `.disabled`, and `focus()`/`blur()` flip `:focus`.
* **Tests:** binding unit tests (host + end-to-end). **DONE 2026-07-17.**

### 7.2 - Event System v1

* **Story 7.2.1: EventTarget + Listener Registry**
* **Goal:** `addEventListener`/`removeEventListener` with capture flag; listener
  lifetimes tied to document teardown.
* **Scope:** per-node listener storage (heap-side, keyed to arena nodes); teardown
  sweep on navigation.
* **Acceptance:** add/remove pairs behave; no stale callbacks after navigation.
* **Tests:** listener lifecycle tests.

* **Story 7.2.2: Event Objects**
* **Goal:** `type`, `target`, `currentTarget`, `preventDefault`, `stopPropagation`,
  key/char fields for keyboard events.
* **Scope:** event struct + JS wrapper (per-dispatch, not retained).
* **Acceptance:** `preventDefault` on submit stops navigation; `stopPropagation`
  halts bubbling.
* **Tests:** event semantics tests.

* **Story 7.2.3: Capture/Target/Bubble Propagation**
* **Goal:** full three-phase dispatch along the ancestor chain.
* **Scope:** dispatch walk + phase bookkeeping.
* **Acceptance:** listener order matches spec for nested capture/bubble combinations.
* **Tests:** event-order integration tests.

* **Story 7.2.4: Input Event Coverage (enumerated)** — *split into 7.2.4.1–7.2.4.4
  (below), which together cover the whole enumerated set. This heading stays as the
  umbrella; the substories are the units of work.*
* **Goal:** route `click`, `keydown`, `keyup`, `input`, `change`, `submit`,
  `dblclick`, `focus`, `blur` from Platform through the DOM dispatch pipeline
  (7.2.3's `dispatch_event`), honoring `preventDefault` on the default action.
* **Scope:** app/engine event routing → DOM dispatch; keep the Platform → Core
  interface boundary intact (platform stays opaque; a new
  `IScriptEngine::dispatch_dom_event(target, {...})` is the seam).
* **Acceptance (whole set):** TodoMVC's Enter-to-add, dblclick-to-edit,
  blur-to-commit flows work; `preventDefault` on a submit stops navigation.
* **Tests:** input-controller + dispatch integration tests (per substory).

* **Story 7.2.4.1: Dispatch Spine + Pointer Events (click, dblclick)**
* **Goal:** stand up the reusable dispatch seam and route real pointer input:
  `IScriptEngine::dispatch_dom_event(DOM::Node* target, {type, bubbles, cancelable,
  key, code}) -> bool` (false ⇒ a listener called `preventDefault`), plumbed
  DocumentScriptController → DocumentPipeline → Tab. A real mouse click
  hit-tests to the topmost DOM node and dispatches a **bubbling, cancelable**
  `click` (and `dblclick` on double-click); `preventDefault` suppresses the
  default action (link navigation, onclick).
* **Why first:** every other substory reuses this seam; click delegation is the
  hn.js secondary-proof path and TodoMVC's destroy/toggle buttons.
* **Acceptance:** a JS `click` listener on an ancestor fires for a click on a
  descendant (delegation); `preventDefault` on a link click stops navigation.
* **Tests:** dispatch-seam unit + click-routing integration tests.

* **Story 7.2.4.2: Keyboard Events (keydown, keyup)**
* **Goal:** route Platform `KeyDown`/`KeyUp` to the focused element (falling back
  to `document`/`body`) as bubbling `keydown`/`keyup` events with `key`/`code`
  populated; `preventDefault` suppresses the default key action (e.g. character
  insertion into a focused field).
* **Why:** TodoMVC's new-todo input is Enter-driven; hn.js reads `key`.
* **Acceptance:** a `keydown` listener sees `key === 'Enter'`; `preventDefault`
  in it stops the default text-edit insertion.
* **Tests:** input-controller + dispatch integration tests.

* **Story 7.2.4.3: Form Input Lifecycle (input, change, focus, blur)**
* **Goal:** fire `input` as the user edits a text field, `change` on commit
  (blur/Enter), and `focus`/`blur` on focus transitions (click-focus,
  programmatic `focus()`/`blur()`, blur-on-commit) — routed from the input
  controller.
* **Why:** TodoMVC's blur-to-commit and edit flows depend on these.
* **Acceptance:** editing a field fires `input`; committing fires `change`;
  focusing/blurring fire `focus`/`blur` on the right element.
* **Tests:** input-controller + dispatch integration tests.

* **Story 7.2.4.4: Submit Event + preventDefault-Stops-Navigation**
* **Goal:** dispatch a cancelable `submit` when a form is submitted (Enter in a
  field / submit button); `preventDefault` halts the form navigation so a JS
  handler can take over.
* **Why:** completes the 7.2.2 acceptance ("preventDefault on submit stops
  navigation"); most JS-driven forms cancel the native submit.
* **Acceptance:** submitting a form fires `submit`; `preventDefault` stops the
  navigation that would otherwise occur.
* **Tests:** form-submit + dispatch integration tests.

* **Story 7.2.5: Fragment Navigation + hashchange**
* **Goal:** `location.hash` read/write, clicking `href="#/..."` links, and the
  `hashchange` event — same-document, **no reload, no document teardown**.
* **Scope:** navigation path special-case for fragment-only URL changes + a minimal
  `window.location` binding (`hash`, `href` read). Full History API stays in M12.
* **Why now:** vanilla TodoMVC's filter bar (All/Active/Completed) is hash-routed;
  without this the "filter" step of the North Star flow silently reloads or 404s.
* **Acceptance:** clicking a `#/active` filter link fires `hashchange`, the handler
  re-renders, and the document (timers, listeners, DOM) survives untouched.
* **Tests:** navigation + event integration tests.

* **Story 7.2.6: Interactive Checkbox Control (T-FORM-CHECKBOX-1)**
* **Split from 7.1.5:** the JS-visible checkbox state (`.checked` get/set) shipped
  with 7.1.5; this story adds the *interactive control* — render + click + the
  events. It lives here (not in 7.1) because its acceptance depends on the event
  pipeline from 7.2.1–7.2.4 (a click must fire `change`/`input`), so it must land
  **after 7.2.4**.
* **Goal:** a working `<input type=checkbox>`: render checked/unchecked, a click
  toggles it, and toggling (by click or by JS setting `.checked`) fires `change`
  (and `input`) through the 7.2 dispatch pipeline. Also wire `element.focus()` to
  the live text-edit caret target (7.1.5 only reflects `:focus` styling).
* **Scope:** a bounded checkbox control — checkbox is currently excluded from input
  semantics in `DocumentInputUtils::is_editable_input_element` and has no
  interactive rendering (M4 forms MVP was text input + button only). Adds: box +
  checkmark paint (`DocumentInputPainter`), hit-test/click-toggle in the input
  controller, and `change`/`input` dispatch. Checkedness stays reflected by the
  `checked` attribute (the 7.1.5 MVP). Radio groups, `<select>`, and everything
  else stay in M11 forms v2.
* **Why it matters:** TodoMVC's per-todo toggle *is* a checkbox whose `change`
  handler drives the re-render; the North Star's toggle step needs this.
* **Acceptance:** a user click toggles the box visibly and fires `change`; setting
  `.checked` from JS updates the rendering; TodoMVC's toggle-complete flow works.
* **Tests:** control interaction tests (click → toggle → `change` dispatched) +
  paint/render tests.

### 7.3 - Scheduling

* **Story 7.3.1: Task Queue (setTimeout/setInterval)**
* **Goal:** timer registration, ordering, `clearTimeout`/`clearInterval`; fired on the
  main-loop tick.
* **Scope:** engine-owned task queue; per-document isolation (timers die with the
  document).
* **Acceptance:** timers fire in order; navigation cancels a document's timers.
* **Tests:** scheduling unit + teardown tests.

* **Story 7.3.2: Microtask Pump (Promise jobs)**
* **Goal:** drain the QuickJS job queue after every script entry point (event
  dispatch, timer callback, eval), before yielding to layout/paint.
* **Scope:** script engine adapter + main-loop integration. *(Hard prerequisite for
  M9's `fetch`.)*
* **Acceptance:** `Promise.resolve().then(...)` runs before the next task; task vs
  microtask interleaving matches spec.
* **Tests:** interleaving-order tests.

* **Story 7.3.3: requestAnimationFrame**
* **Goal:** rAF callbacks run once per frame before paint.
* **Scope:** frame-tick hook; cancellation.
* **Acceptance:** rAF-driven class toggling animates without queue growth.
* **Tests:** frame-scheduling tests.

### 7.4 - Invalidation Model

* **Story 7.4.1: Batched Dirty Marking Per Task**
* **Goal:** mutations mark style/layout/paint dirty; the pipeline runs once per task,
  not per mutation.
* **Scope:** DocumentPipeline dirty flags + flush point at end-of-task.
* **Acceptance:** a handler doing 100 mutations produces exactly one pass (assert via
  instrumentation, in the spirit of M5's injection budget).
* **Tests:** invalidation budget tests.

### 7.5 - Guardrails (standing, start here)

* **Story 7.5.1: TodoMVC Snapshot Harness (T-TODOMVC-E2E-1)**
* **Goal:** pinned vanilla-JS TodoMVC fixture driven headlessly through the full flow.
* **Scope:** fixture + engine/app harness (pattern of M6's T-DDG-E2E-1).
* **Acceptance:** CI fails when the flow regresses.
* **Tests:** integration harness.

* **Story 7.5.2: Missing-API Telemetry (T-JS-REG-1)**
* **Goal:** once-per-page logging of unimplemented JS APIs/DOM properties touched by a
  page (the JS-era T-SUPPORT-REG-1).
* **Scope:** binding-layer trap/registry + report output. **Fail-soft contract:**
  touching a missing API logs once and returns undefined / no-ops — it must not
  throw in a way that aborts the rest of the script (hn.js references `fetch`/XHR
  for voting; that must not prevent the collapse handlers from registering).
* **Acceptance:** loading a proof target emits a deduped missing-API list; this list
  feeds the M12 backlog; a script touching an unimplemented API still runs its
  remaining statements/handlers.
* **Tests:** registry unit tests.

* **Story 7.5.3: Parser Fuzzing In CI (T-FUZZ-1)**
* **Goal:** libFuzzer harnesses for HtmlParser and CssParser with a seed corpus from
  existing fixtures; short deterministic run in CI, longer runs local.
* **Scope:** tooling + CI job; fix-forward policy for findings.
* **Acceptance:** fuzz targets build and run in CI; crashes become regression inputs.
* **Tests:** tooling smoke + accumulated corpus.

* **Story 7.5.4: JS/Native Ownership Rules Documented**
* **Goal:** write down who owns what across the boundary (arena nodes vs JS wrappers
  vs listener registry) before M8/M9 build on it. Include the **wrapper identity
  rule**: the same DOM node must yield the same JS object (`event.target === myEl`
  and `Set`-of-nodes patterns depend on it) — this is a design constraint on the
  binding layer, not a nice-to-have.
* **Scope:** `doc/dev_guide/` entry + assertions where cheap.
* **Acceptance:** rules doc exists; teardown tests reference it.
* **Tests:** teardown/leak tests.

### 7.6 - Browser Chrome (P1, dev-convenience — off the North Star critical path)

* **Story 7.6.1: Back/Forward Navigation (T-UI-NAV-BACK-1)**
* **Goal:** let the user return to the previous page (Alt+Left / a back button)
  instead of being stranded after clicking a link (came up repeatedly while
  evaluating DDG: clicking the logo/results navigates away with no way back).
* **Scope:** a per-tab ordered navigation history stack (the visited-URL set from
  T-HIST-1 is a starting point) + a chrome shortcut/button. Chrome-side only; the
  JS History API stays in M12. Fragment navigations (7.2.5) should push entries so
  back works across hash routes too.
* **Acceptance:** after navigating A→B, back returns to A; forward returns to B.
* **Tests:** tab navigation tests. *(Moved from `doc/TODOs.md` 2026-07-17; filed
  2026-07-16 on user request.)*

* **Story 7.6.2: Bookmarks MVP (T-UI-BOOKMARKS-1)**
* **Goal:** keep the reference pages we test against (html.duckduckgo.com today,
  the TodoMVC/HN fixtures tomorrow) one action away instead of retyped every session.
* **Scope:** a file-backed bookmark list (URL + title) in the user data dir,
  seeded with `https://html.duckduckgo.com/html/`; Ctrl+D bookmarks the current
  page; an internal bookmarks page (e.g. `about:bookmarks`, reachable from the URL
  bar) rendered as plain HTML links through the engine's own pipeline — no new UI
  surface needed. Chrome-side only: no folders, no favicons, no sync, no web-visible
  API.
* **Acceptance:** bookmark a page, restart the browser, open `about:bookmarks`,
  click the entry, land on the page.
* **Tests:** app-level bookmark store tests (persistence + add/dedupe).

---

## Execution Order Checklist

P0: DOM + Events (North Star)
- [x] 7.0.1: External Script Loading (+ T-RESOURCE-TYPE-TABLE-1 paydown) — 2026-07-17
- [x] 7.1.1: Mutation Primitives With Arena Ownership (+ traversal accessors) — 2026-07-17
      (createElement/createTextNode/appendChild/insertBefore/removeChild/replaceChild;
      parentNode/children/childNodes/first-last-child/next-prev-(element-)sibling;
      wrapper identity + `doc/dev_guide/dom_arena_ownership.md`)
- [x] 7.1.2: Attributes, classList, dataset — 2026-07-17
      (getAttribute/removeAttribute + className; classList add/remove/toggle/contains
      via a DOMTokenList; dataset read/write via an exotic DOMStringMap with the
      camelCase<->data-* mapping; class changes feed selector re-match — pipeline
      test shows a JS class toggle restyling an element to display:none)
- [x] 7.1.3: querySelector / querySelectorAll (+ matches/closest/getElementsBy*) — 2026-07-17
      (reuses the CssParser + SelectorMatcher so the supported selector subset is
      identical to CSS; static document-order snapshots; document- and element-scoped;
      matches/closest + legacy getElementsByClassName/getElementsByTagName)
- [x] 7.1.4: innerHTML (fragment parse) — 2026-07-17
      (setter reuses the document HtmlParser in fragment mode — recovery matches
      document parsing; getter serializes children with text/attr escaping and
      void-element handling)
- [x] 7.1.5: Form Control JS Surface — 2026-07-17
      (.value/.checked/.disabled get+set, focus()/blur() reflecting :focus).
      The interactive checkbox control split out to 7.2.6 (needs the event system).
- [x] 7.2.1: EventTarget + Listener Registry — 2026-07-18
      (addEventListener/removeEventListener with capture flag + spec dedupe; per-node
      registry keyed by arena node, each entry owns its JS callback; callbacks freed
      in reset_bindings before wrappers, so none outlive the document. Minimal
      target-phase dispatchEvent for now — real Event objects=7.2.2, capture/bubble=7.2.3.
      document/window as EventTarget deferred to 7.2.3/7.2.4.)
- [x] 7.2.2: Event Objects — 2026-07-18
      (real Event handed to listeners: type/target/currentTarget, key/code, bubbles/
      cancelable; preventDefault→defaultPrevented (dispatchEvent returns false when
      canceled), stopPropagation + stopImmediatePropagation (halts remaining listeners
      on the node). Plain-object flags the C++ dispatch loop reads back — no native
      struct. dispatchEvent accepts a type string or an init object with key/code.)
- [x] 7.2.3: Capture/Target/Bubble Propagation — 2026-07-18
      (three-phase dispatch along the ancestor path target→…→root→document: capture
      down, target (all listeners), bubble up when the event bubbles; consumes
      stopPropagation/stopImmediatePropagation + eventPhase. `document` is now an
      EventTarget (sentinel key) so delegation works — listener order verified for
      nested capture/bubble, incl. document catching a bubbled event.)
- [x] 7.2.4.1: Dispatch spine + pointer events (click, dblclick) — 2026-07-18
      (`IScriptEngine::dispatch_dom_event(target, {type,bubbles,cancelable,key,code})→bool`
      seam; threaded controller→scripting→pipeline→tab→app router; real click hit-tests
      the topmost node and dispatches a bubbling/cancelable `click` (+`dblclick` on
      double, via SDL button.clicks); preventDefault reported up and gates link nav.
      Delegation + dblclick + preventDefault verified through the real pipeline.)
- [x] 7.2.4.2: Keyboard events (keydown, keyup) on the focused element — 2026-07-18
      (route Platform KeyDown/KeyUp → focused element (else document) as bubbling
      keydown/keyup with key/code; preventDefault on keydown suppresses the default
      text-edit; a mutating key listener rebuilds the doc. New KeyUp routing path.)
- [ ] 7.2.4.3: Form input lifecycle (input, change, focus, blur)
- [ ] 7.2.4.4: Submit event + preventDefault stops navigation
- [ ] 7.2.6: Interactive Checkbox Control (render + click-toggle + change/input) — split from 7.1.5; needs 7.2.1–7.2.4
- [ ] 7.2.5: Fragment Navigation + hashchange

P0: Scheduling + Invalidation (North Star)
- [ ] 7.3.1: Task Queue (setTimeout/setInterval)
- [ ] 7.3.2: Microtask Pump (Promise jobs)
- [ ] 7.4.1: Batched Dirty Marking Per Task

P0: Guardrails
- [ ] 7.5.4: JS/Native Ownership Rules Documented
- [ ] 7.5.1: TodoMVC Snapshot Harness
- [ ] 7.5.2: Missing-API Telemetry (fail-soft)
- [ ] 7.5.3: Parser Fuzzing In CI
- [ ] Secondary proof: HN item-page snapshot — comment collapse works

P1: If Schedule Allows
- [ ] 7.3.3: requestAnimationFrame
- [ ] 7.6.1: Back/Forward Navigation (T-UI-NAV-BACK-1)
- [ ] 7.6.2: Bookmarks MVP (T-UI-BOOKMARKS-1)
