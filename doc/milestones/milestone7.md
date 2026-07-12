> **Status: Planned** — pre-written 2026-07, two milestones ahead. Sanity-check scope
> against the M6 outcome before kickoff; story structure is expected to hold.

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

---

## Non-Goals (keep the blast radius controlled)

* No fetch/XHR or any networking JS API (M9). No cookies/storage bindings (M8).
* No History API, no observer APIs (`MutationObserver` etc.), no custom elements (M12).
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

* DOM construction/mutation primitives with arena ownership (7.1.1) + `innerHTML` (7.1.4).
* `querySelector`/`querySelectorAll` over the existing SelectorMatcher (7.1.3).
* EventTarget with propagation and real event objects (7.2.1–7.2.3).
* Keyboard/input/submit event routing (7.2.4) — TodoMVC is keyboard-driven.
* Task + microtask queues in the main loop (7.3.1–7.3.2).
* Batched invalidation per task (7.4.1).
* TodoMVC snapshot harness in CI (7.5.1).

**Nice-to-have (if schedule allows)**

* `requestAnimationFrame` (7.3.3) — cheap once the frame tick owns the queues.
* `classList`/`dataset` conveniences beyond what TodoMVC touches (7.1.2 minimum scope
  is what the target needs).

---

## Milestone 7 Done When

* TodoMVC (pinned snapshot) passes a scripted add/toggle/edit/filter/clear flow in the
  headless harness, and CI fails on regression.
* Capture/target/bubble order is correct for nested listeners (event-order tests).
* A handler performing many mutations triggers exactly one style/layout/paint pass.
* `setTimeout(fn, 0)` and Promise jobs interleave in spec order (task vs microtask
  tests).
* Missing-API telemetry and parser fuzzing run in CI (standing guardrails start here).
* JS/native ownership rules are documented (`doc/dev_guide/`), and teardown tests show
  no stale listeners or dangling node references after navigation.

---

## Stories

### 7.1 - DOM Core (JS can build UI)

* **Story 7.1.1: Mutation Primitives With Arena Ownership**
* **Goal:** `createElement`, `createTextNode`, `appendChild`, `insertBefore`,
  `removeChild`, `replaceChild`.
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
* **Goal:** subtree selector queries reusing SelectorMatcher.
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

* **Story 7.2.4: Input Event Coverage (enumerated)**
* **Goal:** route `keydown`, `keyup`, `input`, `change`, `submit`, `dblclick`,
  `focus`, `blur` from Platform through the dispatch pipeline.
* **Scope:** app/engine event routing → DOM dispatch; keep the Platform → Core
  interface boundary intact.
* **Acceptance:** TodoMVC's Enter-to-add, dblclick-to-edit, blur-to-commit flows work.
* **Tests:** input-controller + dispatch integration tests.

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
* **Scope:** binding-layer trap/registry + report output.
* **Acceptance:** loading a proof target emits a deduped missing-API list; this list
  feeds the M12 backlog.
* **Tests:** registry unit tests.

* **Story 7.5.3: Parser Fuzzing In CI (T-FUZZ-1)**
* **Goal:** libFuzzer harnesses for HtmlParser and CssParser with a seed corpus from
  existing fixtures; short deterministic run in CI, longer runs local.
* **Scope:** tooling + CI job; fix-forward policy for findings.
* **Acceptance:** fuzz targets build and run in CI; crashes become regression inputs.
* **Tests:** tooling smoke + accumulated corpus.

* **Story 7.5.4: JS/Native Ownership Rules Documented**
* **Goal:** write down who owns what across the boundary (arena nodes vs JS wrappers
  vs listener registry) before M8/M9 build on it.
* **Scope:** `doc/dev_guide/` entry + assertions where cheap.
* **Acceptance:** rules doc exists; teardown tests reference it.
* **Tests:** teardown/leak tests.

---

## Execution Order Checklist

P0: DOM + Events (North Star)
- [ ] 7.1.1: Mutation Primitives With Arena Ownership
- [ ] 7.1.2: Attributes, classList, dataset
- [ ] 7.1.3: querySelector / querySelectorAll
- [ ] 7.1.4: innerHTML (fragment parse)
- [ ] 7.2.1: EventTarget + Listener Registry
- [ ] 7.2.2: Event Objects
- [ ] 7.2.3: Capture/Target/Bubble Propagation
- [ ] 7.2.4: Input Event Coverage

P0: Scheduling + Invalidation (North Star)
- [ ] 7.3.1: Task Queue (setTimeout/setInterval)
- [ ] 7.3.2: Microtask Pump (Promise jobs)
- [ ] 7.4.1: Batched Dirty Marking Per Task

P0: Guardrails
- [ ] 7.5.4: JS/Native Ownership Rules Documented
- [ ] 7.5.1: TodoMVC Snapshot Harness
- [ ] 7.5.2: Missing-API Telemetry
- [ ] 7.5.3: Parser Fuzzing In CI

P1: If Schedule Allows
- [ ] 7.3.3: requestAnimationFrame
