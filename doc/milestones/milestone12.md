> **Status: Aspirational** — created 2026-07-26 by triaging `doc/TODOs.md` at the M9
> kickoff. **This is a thinner draft than milestone9/10/11 on purpose.** M12's scope
> is meant to be driven by *missing-API telemetry* from the proof target rather than
> guessed in advance, so this doc records the items we already know about and the
> shape of the milestone, and leaves the long tail to be filled in from the log.
> Revalidate everything at kickoff.

## Milestone 12 North Star Deliverable

**Before (after Milestone 11):**

* The engine can fetch, render, position, scroll, and accept text input — but it
  cannot survive a framework. `customElements.define()` does nothing, so a
  Polymer/Stencil page renders as unknown tags. There are no observer APIs, so
  anything that lazy-loads or reacts to resize never initializes.
* M9 shipped a History API **MVP** (`pushState`/`replaceState`/`popstate` with URL-bar
  and history-entry updates); the surrounding SPA-navigation model — scroll
  restoration, navigation interception, same-document edge cases — is still absent.
* Feeds grow the DOM without bound: there is no virtualization and no resource
  eviction for long-running pages.

**After (Milestone 12 done):**

* **Custom Elements:** `customElements.define()` upgrade path with lifecycle
  callbacks.
* **Observer APIs:** `MutationObserver`, `IntersectionObserver` (feeds and
  lazy-loading depend on it), `ResizeObserver`.
* **SPA navigation completed** on top of M9's History API MVP.
* **Long-tail bindings** driven by telemetry, not by guesswork.
* **Proof target:** **m.youtube.com renders its app shell** — thumbnails, navigation
  between pages — with video playback explicitly out of scope.

---

## How this milestone gets planned

The standing missing-API telemetry guardrail (from M7) logs every JS API and DOM
property a proof target touches that the engine does not implement. By the time M12
starts, three proof-target runs will have contributed to it (M9's API-render targets,
M10's Wikipedia/old.reddit, M11's login target), plus whatever the m.youtube shell
reports on first contact.

**Kickoff procedure:** run the proof target, dump the telemetry, sort by frequency,
and turn the top of that list into stories. The items below are what we already know
belongs here — they are the floor, not the ceiling.

---

## Non-Goals

* No video playback, no MSE, no adaptive streaming (M13/M15).
* No compositor or 60fps guarantee (M14).
* No accessibility tree (unscheduled).

---

## Known Stories

### 12.1 - Custom Elements

* **Story 12.1.1: Custom Elements Upgrade (T-DOM-CUSTOM-1)** [P1]
* **Goal:** allow JS `customElements.define()` to upgrade dash-named tags and run
  lifecycle hooks.
* **Scope:** DOM + JS bindings.
* **Acceptance:** a defined custom element runs `constructor`/`connectedCallback` and
  can attach shadow/DOM.
* **Tests:** DOM/JS integration tests.
* *Required for YouTube, which is Polymer — this is not optional for the endgame.*

### 12.2 - Observer APIs

* **Story 12.2.1: `MutationObserver`** *(roadmap headline; not previously ticketed)*
* **Goal:** report DOM mutations asynchronously to script.
* **Scope:** rides M7's mutation epoch and microtask pump — the invalidation model
  already tracks what changed per task, which is the hard half.
* **Acceptance:** a registered observer receives records for childList/attributes/
  characterData mutations, delivered as a microtask.
* **Tests:** observer delivery + batching tests.

* **Story 12.2.2: `IntersectionObserver`** *(roadmap headline)*
* **Goal:** notify script when an element enters or leaves the viewport.
* **Scope:** needs M10's scroll containers to be meaningful — intersection is
  computed against a scroll root. Feeds and lazy-loading depend on it, so this is
  the observer that unblocks the m.youtube shell's thumbnails.
* **Acceptance:** an observed element below the fold reports `isIntersecting: false`,
  then true after scrolling to it.
* **Tests:** intersection tests over a scroll container.

* **Story 12.2.3: `ResizeObserver`** *(roadmap headline)*
* **Goal:** notify script when an element's box changes size.
* **Scope:** stub → real as telemetry demands. Cheapest correct staging is to fire
  from the existing layout-pass completion hook.
* **Acceptance:** a resize of the viewport or a container delivers a record.
* **Tests:** resize delivery tests.

### 12.3 - SPA Navigation (completing M9's MVP)

* **Story 12.3.1: SPA Navigation Model** *(roadmap headline; builds on M9's 9.6.1)*
* **Goal:** complete the same-document navigation model M9 started.
* **Scope:** what 9.6.1 explicitly deferred — scroll restoration (needs M10's scroll
  containers), navigation interception, same-document navigation edge cases,
  `history.state` serialization beyond structured-clone-of-JSON, and interaction
  between route changes and scroll position. Also the M10 prerequisite the roadmap
  names: a stable document lifecycle across back/forward with no leaks and no
  dangling `string_view`.
* **Acceptance:** a framework router drives multi-step navigation with correct
  back/forward and restored scroll positions, without document teardown.
* **Tests:** navigation-model tests + the framework proof target.

### 12.4 - Event System Completion

* **Story 12.4.1: Event System v1 Known Spec Deviations
  (T-EVENT-SPEC-GAPS-1)** [P3]
* **Goal:** close (or consciously re-defer) the deliberate MVP deviations recorded
  when the event system shipped, rather than rediscovering them.
* **Scope:** five enumerated items:
  1. **mouse-down ordering** — the `click` dispatch runs before the focus
     transition's `blur`/`change`/`focus` (browsers fire blur/change on mousedown,
     *before* click); see `DocumentEventRouter::handle_mouse_down`.
  2. **`window` is not on the propagation path** — bubbling events stop at
     `document`, so `window.addEventListener('click')` never fires (`hashchange`
     targets window directly and works).
  3. **string-form `el.dispatchEvent('x')` defaults `cancelable` to true** (spec:
     false) — a ScriptEngine test currently locks this in.
  4. **no `Event`/`CustomEvent` constructors** — pages that `new CustomEvent(...)`
     cannot synthesize events.
  5. **`addEventListener` ignores `once`/`passive`/`signal`** options.
* **Acceptance:** each item either implemented or explicitly re-deferred.
* **Tests:** per item.
* *Filed 2026-07-18; **was tagged M8, re-homed here at the M9 kickoff.** Items (2),
  (4), and (5) are framework surface — `window` listeners, `CustomEvent`, and
  listener options are exactly what a framework's event plumbing assumes — so this is
  the right milestone. Item (4) was already flagged as "the most likely to bite on
  real pages"; expect telemetry to confirm it.*

### 12.5 - Unbounded Feeds

* **Story 12.5.1: Infinite Scroll DOM Virtualization (T-DOM-1)** [P1]
* **Goal:** cap live DOM and resources for unbounded feeds.
* **Scope:** DOM/layout + resource eviction.
* **Acceptance:** long feeds do not grow memory unbounded, and rehydrate when
  revisiting content.
* **Tests:** engine perf tests.
* *Moved out of M6 in 2026-07-16 as "pull in only if real pages force it"; needs an
  SPA/infinite feed to force and validate it, and m.youtube's app shell is exactly
  that. Depends on 12.2.2 (`IntersectionObserver`) and M10's scroll containers.*

### 12.6 - Cosmetic Long Tail

* **Story 12.6.1: Multi-Column (T-CSS-MULTICOL-1)** [P3]
* **Goal:** flow block content into multiple columns.
* **Scope:** `columns`/`column-count`/`column-width`/`column-gap` warn "unsupported"
  and content renders single-column. Implement column fragmentation with count/width
  and gap.
* **Acceptance:** `column-count:2` splits flow content into two balanced columns.
* **Tests:** multicol layout tests.
* *Rare on the critical-path targets and used only lightly by seznam — lowest-priority
  layout item; scheduled late unless a proof target forces it sooner.*

* **Story 12.6.2: Decorative CSS No-Ops — Tracked Bundle
  (T-CSS-VISUAL-EFFECTS-1)** [P3]
* **Goal:** keep the purely-decorative properties the engine renders blind on a
  tracked list, so each is a conscious pull-in rather than a rediscovery.
* **Scope:** one grab-bag ticket for `mask`/`mask-image`, `filter`,
  `backdrop-filter`, `mix-blend-mode`, `background-clip`, `clip-path`,
  `will-change`, `user-select`, `caret-color`, `scrollbar-width`/`scrollbar-gutter`,
  and the `scroll-*` niceties (`scroll-behavior`, `scroll-snap-*`, `scroll-padding`,
  `overscroll-behavior`, `overflow-anchor`) — all seen warning on seznam, none of
  which affect whether a page is navigable. Pull an individual property into its own
  story only when a proof target visibly needs it.
* **Acceptance:** n/a (tracking bucket) — the register in `doc/conformance` names each
  and its status.
* **Tests:** per property when promoted.
* **Note — CSS Transitions and Animations** (`animation*`, `transition*`,
  `@keyframes`) are also unimplemented and **not tracked anywhere**. They are a
  subsystem of their own, best decided with the M14 compositor work. Flagged here; to
  be filed properly at the M14 kickoff. Several `scroll-*` items above also become
  meaningful only once M10's scroll containers exist.

---

## Execution Order Checklist

*Provisional — re-derive from telemetry at kickoff.*

P0: Framework survival
- [ ] 12.1.1: Custom Elements Upgrade
- [ ] 12.2.1: `MutationObserver`
- [ ] 12.2.2: `IntersectionObserver`
- [ ] 12.3.1: SPA Navigation Model

P1: Shell viability
- [ ] 12.2.3: `ResizeObserver`
- [ ] 12.5.1: Infinite Scroll DOM Virtualization
- [ ] Long-tail bindings from missing-API telemetry *(write up at kickoff)*

P3: Long tail
- [ ] 12.4.1: Event System Known Spec Deviations
- [ ] 12.6.1: Multi-Column
- [ ] 12.6.2: Decorative CSS No-Ops (tracked bundle)
