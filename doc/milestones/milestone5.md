## Milestone 5 North Star Deliverable

**Before (after Milestone 4):**

* Browser can render and navigate basic pages, with partial JS and partial CSS support.
* No true browser-managed extensibility layer yet (tabs/plugins/background behavior across pages is missing).
* DDG HTML is still not reliably usable end-to-end (typing/submission gaps remain).

**After (Milestone 5 done):**

* Browser has a working **Browser OS layer**:
  * multi-tab management,
  * extension loading,
  * long-lived background scripts,
  * extension API for tab events + CSS injection.
* Built-in **Dark Mode extension** can inject CSS into every loaded page.
* **DuckDuckGo HTML mode is usable end-to-end**:
  * focus search field,
  * type query,
  * submit (Enter/click),
  * navigate to results.

---

## Non-Goals (keep the blast radius controlled)

* No extension marketplace, signing, permission prompts, or security sandbox model.
* No broad WebExtension compatibility target (only a narrow `browser.*` MVP).
* No full modern web compatibility target.
* No major threading model changes (DOM/layout/style stays main-thread deterministic).

---

## Critical Path (what must land for the North Star)

**Must-have**

* **Tabs as first-class runtime objects** (create/close/switch active tab, per-tab isolation).
* **Extension host MVP** (manifest + loader + background runtime).
* **Minimal `browser.*` API** for tab lifecycle events and CSS injection.
* **CSS injection layer in cascade** with deterministic invalidation.
* **Built-in Dark Mode extension** proving end-to-end injection.
* **DDG HTML completion track**: input focus/edit + submit + navigation.

**Nice-to-have (if schedule allows)**

* Enable/disable extension UI toggle (config/CLI is enough for MVP).
* `removeCSS` support (if dark-mode toggling needs cleanup semantics).
* Additional extension diagnostics tooling.

---

## Milestone 5 Done When

Milestone 5 is complete when all items below are true:

* Multi-tab workflow works (create/switch/close) with per-tab isolation.
* Extension host loads at least one extension and runs a background script.
* `browser.tabs` events and CSS injection API work end-to-end.
* Built-in Dark Mode extension applies across loaded pages.
* DDG HTML homepage supports real typing and submit flow to results.
* Regression tests cover extension API flow and DDG interaction flow.

---

## Numbered Stories (5.x.y)

### 5.1 - Browser OS Skeleton (Tabs)

* **Story 5.1.1: TabManager + Multi-Tab Model**
* **Goal:** Engine owns multiple `Tab` instances and tracks one active tab.
* **Scope:** tab creation, close, switch, active index/id model.
* **Acceptance:** user can open at least two tabs and switch between them without state bleed.
* **Tests:** app/engine tab-management tests.

* **Story 5.1.2: Per-Tab Isolation Contract**
* **Goal:** each tab has isolated document/resource/script state.
* **Scope:** ensure no DOM/style/resource/script leakage across tabs.
* **Acceptance:** edits/actions in tab A do not mutate tab B.
* **Tests:** engine isolation tests.

* **Story 5.1.3: Minimal Tab UI + Shortcuts**
* **Goal:** basic tab strip and keyboard actions (`new`, `next`, `prev`, `close`).
* **Scope:** minimal UI is acceptable (text-only strip).
* **Acceptance:** manual flow can create/switch/close tabs.
* **Tests:** focused app tests where feasible; otherwise smoke/manual checklist.

---

### 5.2 - Extension Host MVP

* **Story 5.2.1: Extension Manifest v0**
* **Goal:** define minimal package format (`name`, `version`, `background.entry`, permission placeholder).
* **Scope:** parser/validator + error reporting.
* **Acceptance:** invalid manifests fail with actionable logs; valid manifests load.
* **Tests:** parser/loader unit tests.

* **Story 5.2.2: Extension Loader**
* **Goal:** load extensions from configured directory (or built-in bundle path).
* **Scope:** discovery, manifest parse, startup ordering.
* **Acceptance:** at least one extension loads automatically at browser startup.
* **Tests:** integration tests for load success/failure cases.

* **Story 5.2.3: Background Script Runtime**
* **Goal:** one long-lived QuickJS runtime/context per extension.
* **Scope:** init, eval, error surfacing, teardown.
* **Acceptance:** background script stays alive across tab navigations.
* **Tests:** script host lifecycle tests.

* **Story 5.2.4: Enable/Disable Lifecycle**
* **Goal:** deterministic extension enable/disable behavior.
* **Scope:** config/CLI first, UI optional.
* **Acceptance:** disabled extension does not receive events or inject CSS.
* **Tests:** lifecycle tests.

---

### 5.3 - `browser.*` API v0

* **Story 5.3.1: `browser.tabs` Events + Active Query**
* **Goal:** expose tab lifecycle events and a minimal active-tab query API.
* **Scope:** `onCreated`, `onNavigated` (or `onUpdated`), `onActivated`, active lookup.
* **Acceptance:** background script can react to tab navigation and identify target tab.
* **Tests:** extension API tests.

* **Story 5.3.2: CSS Injection Primitive**
* **Goal:** expose `insertCSS({tabId, cssText})` (and optionally `removeCSS`).
* **Scope:** API surface, validation, routing into tab/style pipeline.
* **Acceptance:** extension can inject CSS into a specified tab deterministically.
* **Tests:** API + style integration tests.

* **Story 5.3.3: Deterministic Event Wiring**
* **Goal:** emit extension events on document commit/navigation transitions (not per-frame polling).
* **Scope:** tab/document pipeline hooks.
* **Acceptance:** one event per navigation commit.
* **Tests:** event-order integration tests.

---

### 5.4 - CSS Injection Integration (Engine-Side)

* **Story 5.4.1: Extension Stylesheet Origin Layer**
* **Goal:** add extension/user stylesheet layer with documented cascade order.
* **Scope:** cascade origin ordering and conflict behavior.
* **Acceptance:** extension styles apply consistently per chosen origin precedence.
* **Tests:** style cascade tests.

* **Story 5.4.2: Per-Tab Injected Stylesheet Store**
* **Goal:** track injected CSS per tab/document and apply single invalidation per injection.
* **Scope:** storage model + invalidation integration.
* **Acceptance:** one injection triggers one style/layout/paint pass, not repeated rebuild loops.
* **Tests:** document pipeline/invalidation tests.

* **Story 5.4.3: Built-In Dark Mode Extension**
* **Goal:** ship canonical demo extension.
* **Scope:** background script + curated CSS set.
* **Acceptance:** dark mode visibly applies across navigations and tab switches.
* **Tests:** integration test + manual visual verification.

---

### 5.5 - DDG HTML Completion Track (Explicit M5 Goal)

This epic is mandatory for Milestone 5 closure.

* **Story 5.5.1: Input Focus/Hit-Test Reliability on Real Pages**
* **Goal:** ensure text inputs receive focus even with real-world positioned overlays.
* **Scope:** hit-test ordering, z-order interactions, replaced-element focus path.
* **Acceptance:** DDG input can be focused by click reliably.
* **Tests:** engine/layout hit-test tests with DDG-like structure.

* **Story 5.5.2: `autofocus` Behavior**
* **Goal:** honor `autofocus` for initial focus when document loads.
* **Scope:** document load focus policy.
* **Acceptance:** DDG search box is focused at load when appropriate.
* **Tests:** document/input controller tests.

* **Story 5.5.3: Submit Controls Parity (`<input type="submit">`)**
* **Goal:** support submit behavior for input-submit controls (not just `<button>`).
* **Scope:** form association + click/keyboard submit paths.
* **Acceptance:** DDG submit input triggers navigation.
* **Tests:** form submit tests for input-submit.

* **Story 5.5.4: Form `method="post"` Support (MVP)**
* **Goal:** support POST form submission flow (including request body encoding baseline).
* **Scope:** form serialization + network request path for POST.
* **Acceptance:** DDG form submission path works with POST.
* **Tests:** URL/form serialization + engine network tests.

* **Story 5.5.5: DDG Homepage Layout/Usability Polish**
* **Goal:** keep search field/button visually aligned and usable on load.
* **Scope:** targeted CSS/layout gaps that block interaction.
* **Acceptance:** user can type and submit on DDG HTML homepage without workaround.
* **Tests:** visual regression checklist + targeted layout tests.

---

### 5.6 - Tests, Guardrails, and Performance Hygiene

* **Story 5.6.1: Extension API Headless Coverage**
* **Goal:** test event -> injection -> computed style change path.
* **Acceptance:** deterministic regression tests cover the core extension loop.

* **Story 5.6.2: Tab/Extension Teardown Coverage**
* **Goal:** verify close/reset/disable lifecycle correctness.
* **Acceptance:** no double-shutdown, no stale callbacks, no leaks across tab/extension teardown.

* **Story 5.6.3: Dependency Firewall Audit**
* **Goal:** keep Ports & Adapters boundaries intact while adding extension/tab layers.
* **Acceptance:** no new forbidden dependency edges.

* **Story 5.6.4: Injection Invalidation Budget**
* **Goal:** assert CSS injection causes bounded invalidation (single pass per injection).
* **Acceptance:** no per-frame rebuild behavior from extension CSS.

---

## Named Stories (T-*)

These are Milestone 5 candidates that are not duplicated by the numbered 5.x.y stories above.

* **Story T-ARCH-CYCLE-1 (M5 P1): Break Engine Document/Script Cycle**
* **Goal:** remove direct package cycle between `engine/document` and `engine/script`.
* **Scope:** introduce narrow script-facing document interfaces and invert dependencies.
* **Acceptance:** package graph no longer has `document <-> script` cycle.
* **Tests:** engine + script tests unchanged.

* **Story T-ARCH-CYCLE-2 (M5 P1): Break PlatformApi/Geometry Cycle**
* **Goal:** decouple `core/platform_api` from `layout/geometry`.
* **Scope:** move shared rect/point POD types into a neutral core geometry/types module and update interfaces.
* **Acceptance:** package graph no longer has `platform_api <-> geometry` cycle.
* **Tests:** app/engine/layout tests unchanged.

* **Story T-ARCH-INCLUDE-1 (M5 P1): Slim Tab.h and DocumentPipeline.h Includes**
* **Goal:** reduce header fan-out and rebuild impact.
* **Scope:** forward declarations + move heavy includes to `.cpp` for `Tab` and `DocumentPipeline`.
* **Acceptance:** include graph fan-out for these headers drops materially and no behavior change.
* **Tests:** engine tests.

* **Story T-REF-ENGINE-1 (M5 P1): Reshuffle Engine Modules**
* **Goal:** group engine files by domain (document/tab/resources).
* **Scope:** Engine folder structure + namespaces.
* **Acceptance:** clearer module layout with minimal includes.
* **Tests:** existing engine tests.

* **Story T-PERF-3 (M5 P1): Split UI Chrome From Page Render**
* **Goal:** avoid repainting page while editing URL bar.
* **Scope:** app render split.
* **Acceptance:** URL bar updates without page repaint.
* **Tests:** manual.

* **Story T-LAYOUT-INLINE-2 (M5 P1): Inline-Block Baseline Alignment**
* **Goal:** inline-block aligns to text baseline by default.
* **Scope:** inline layout + line box metrics.
* **Acceptance:** inline-block does not appear to "sink" below text.
* **Tests:** layout tests.

* **Story T-CSS-SEL-2 (M5 P1): Child combinator selector (`>`)**
* **Goal:** support direct-child selector matching.
* **Scope:** CssParser + SelectorMatcher.
* **Acceptance:** `.parent > .child` matches direct children only.
* **Tests:** selector matcher tests.

* **Story T-CSS-VIS-1 (M5 P1): opacity (paint-only)**
* **Goal:** apply opacity in paint.
* **Scope:** Painter.
* **Acceptance:** subtree alpha scales paint.
* **Tests:** renderer tests.

* **Story T-ARCH-SPLIT-1 (M5 P2): DocumentPipeline Responsibility Split**
* **Goal:** keep pipeline orchestration thin.
* **Scope:** extract focused coordinators/services for link hit-test/form submit/script dispatch/resource apply flow.
* **Acceptance:** `DocumentPipeline` public API and internals are slimmer with clear boundaries and unchanged behavior.
* **Tests:** document pipeline + tab tests.

* **Story T-ARCH-SPLIT-2 (M5 P2): ResourceLoader Decomposition**
* **Goal:** prevent growth of a networking god-object.
* **Scope:** split request planning, response integration, and policy (fallback/insecure) into dedicated helpers.
* **Acceptance:** `ResourceLoader` complexity reduced and behavior preserved.
* **Tests:** resource loader + tab tests.

* **Story T-UI-FORM-1 (M5 P2): Form Control Styling Polish**
* **Goal:** native-like input/button visuals (shading, hover, pressed).
* **Scope:** renderer + style defaults.
* **Acceptance:** inputs/buttons look intentional and stateful.
* **Tests:** manual.

* **Story T-CSS-BORDER-2 (M5 P2): border-radius (paint)**
* **Goal:** round corners.
* **Scope:** Painter.
* **Acceptance:** rounded rect paint for background/border.
* **Tests:** renderer tests.

* **Story T-CSS-DECOR-1 (M5 P2): outline + outline-offset**
* **Goal:** draw outlines.
* **Scope:** Painter.
* **Acceptance:** outline draws outside border with offset.
* **Tests:** renderer tests.

* **Story T-CSS-TEXT-1 (M5 P2): Text Effects Polish**
* **Goal:** support `text-transform`, `letter-spacing`, `text-indent`, `text-overflow`, `word-wrap`.
* **Scope:** style + text layout/painter.
* **Acceptance:** long labels elide/wrap closer to author CSS.
* **Tests:** renderer + layout tests.

* **Story T-IMG-1 (M5 P2): Animated GIF/WebP Playback**
* **Goal:** play animated frames.
* **Scope:** decoder + renderer scheduling.
* **Acceptance:** frames render with timing.
* **Tests:** image tests.

* **Story T-IMG-2 (M5 P2): SVG Image Decode (Raster)**
* **Goal:** rasterize SVG `<img>` sources.
* **Scope:** IImageDecoder + SVG library.
* **Acceptance:** svg renders to ImageBitmap.
* **Tests:** image tests.

* **Story T-FORM-3 (M5 P3): URL-encoded + spaces**
* **Goal:** use application/x-www-form-urlencoded space encoding.
* **Scope:** url encoding.
* **Acceptance:** spaces become `+`.
* **Tests:** core utils tests.

* **Story T-ARCH-CYCLE-3 (M5 P3): Break Dom/StyleCompute Cycle**
* **Goal:** remove `core/dom <-> style/compute` mutual dependency.
* **Scope:** isolate style-facing DOM access behind read-only adapter/traits interfaces.
* **Acceptance:** package graph no longer has `dom <-> compute` cycle.
* **Tests:** style + layout + DOM tests.

* **Story T-CSS-CODE-1 (M5 P3): Code/Pre Background Defaults**
* **Goal:** default inline code/pre background should not fight page background.
* **Scope:** style defaults + computed style.
* **Acceptance:** code/pre render transparent unless author CSS sets a background.
* **Tests:** style tests.

* **Story T-LIST-1 (M5 P3): Ordered List Markers**
* **Goal:** ordered lists show numeric markers instead of bullets.
* **Scope:** list marker layout/paint.
* **Acceptance:** `<ol>` renders `1.`, `2.`, `3.`.
* **Tests:** layout tests.

* **Story T-TABLE-1 (M5 P3): Table Borders for Visibility**
* **Goal:** make table structure visible without author CSS.
* **Scope:** style defaults for `table/td/th` or table paint.
* **Acceptance:** tables show cell boundaries in demo.
* **Tests:** renderer tests.

* **Story T-HTML-SEM-2 (M5 P3): Semantic Landmark Roles (A11y)**
* **Goal:** expose `<header/nav/main/section/article/aside/footer>` semantics for accessibility and tooling.
* **Scope:** DOM semantics + a11y hooks.
* **Acceptance:** semantic tags report correct roles.
* **Tests:** DOM/a11y tests.

---

## Execution Order Checklist

This is the single recommended execution checklist, mixing numbered and named stories.

P0: Browser OS + Extensions (North Star)
- [x] 5.1.1: TabManager + Multi-Tab Model
- [x] 5.1.2: Per-Tab Isolation Contract
- [x] 5.1.3: Minimal Tab UI + Shortcuts
- [x] 5.2.1: Extension Manifest v0
- [x] 5.2.2: Extension Loader
- [x] 5.2.3: Background Script Runtime
- [x] 5.2.4: Enable/Disable Lifecycle
- [ ] 5.3.3: Deterministic Event Wiring
- [x] 5.3.1: `browser.tabs` Events + Active Query
- [ ] 5.4.1: Extension Stylesheet Origin Layer
- [ ] 5.4.2: Per-Tab Injected Stylesheet Store
- [ ] 5.3.2: CSS Injection Primitive
- [ ] 5.6.4: Injection Invalidation Budget
- [ ] 5.4.3: Built-In Dark Mode Extension

P0: Tests and Guardrails
- [ ] 5.6.1: Extension API Headless Coverage
- [ ] 5.6.2: Tab/Extension Teardown Coverage
- [ ] 5.6.3: Dependency Firewall Audit

P0: DDG HTML End-to-End Usability (Mandatory)
- [ ] 5.5.1: Input Focus/Hit-Test Reliability on Real Pages
- [ ] 5.5.2: `autofocus` Behavior
- [ ] 5.5.3: Submit Controls Parity (`<input type="submit">`)
- [ ] 5.5.4: Form `method="post"` Support (MVP)
- [ ] T-FORM-3 (M5 P3): URL-encoded + spaces
- [ ] 5.5.5: DDG Homepage Layout/Usability Polish

P1+: Refactors and Polish (Only if Needed / Time Allows)
- [ ] T-ARCH-INCLUDE-1 (M5 P1): Slim Tab.h and DocumentPipeline.h Includes
- [ ] T-REF-ENGINE-1 (M5 P1): Reshuffle Engine Modules
- [ ] T-ARCH-CYCLE-1 (M5 P1): Break Engine Document/Script Cycle
- [ ] T-ARCH-CYCLE-2 (M5 P1): Break PlatformApi/Geometry Cycle
- [ ] T-ARCH-SPLIT-1 (M5 P2): DocumentPipeline Responsibility Split
- [ ] T-ARCH-SPLIT-2 (M5 P2): ResourceLoader Decomposition
- [ ] T-ARCH-CYCLE-3 (M5 P3): Break Dom/StyleCompute Cycle
- [ ] T-PERF-3 (M5 P1): Split UI Chrome From Page Render
- [ ] T-LAYOUT-INLINE-2 (M5 P1): Inline-Block Baseline Alignment
- [ ] T-CSS-SEL-2 (M5 P1): Child combinator selector (`>`)
- [ ] T-CSS-VIS-1 (M5 P1): opacity (paint-only)
- [ ] T-CSS-BORDER-2 (M5 P2): border-radius (paint)
- [ ] T-CSS-DECOR-1 (M5 P2): outline + outline-offset
- [ ] T-CSS-TEXT-1 (M5 P2): Text Effects Polish
- [ ] T-CSS-CODE-1 (M5 P3): Code/Pre Background Defaults
- [ ] T-LIST-1 (M5 P3): Ordered List Markers
- [ ] T-TABLE-1 (M5 P3): Table Borders for Visibility
- [ ] T-IMG-1 (M5 P2): Animated GIF/WebP Playback
- [ ] T-IMG-2 (M5 P2): SVG Image Decode (Raster)
- [ ] T-UI-FORM-1 (M5 P2): Form Control Styling Polish
- [ ] T-HTML-SEM-2 (M5 P3): Semantic Landmark Roles (A11y)
