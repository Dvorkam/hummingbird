## Milestone 6 North Star Deliverable

**Before (after Milestone 5):**

* Browser has a working Browser OS layer: multi-tab, extensions, background scripts, CSS injection, Dark Mode.
* DDG HTML mode is *usable* end-to-end (focus/type/submit/navigate), but the page does not *look* right:
  * logo/search block is not centered (no flexbox),
  * control chrome (borders, corner radii) diverges from reference browsers,
  * legacy/vendor-prefixed CSS produces noise and visual gaps.
* DDG regressions are guarded only by manual checking, not CI (note: the `doc/checklists/` files referenced by earlier TODOs were never committed — recreate the DDG checklist as part of this milestone).
* Package-cycle hygiene from M5 refactors has no automated enforcement.

**After (Milestone 6 done):**

* **Flexbox v1** works with enough fidelity for real-page centering/alignment flows.
* **DDG HTML homepage visually matches a reference browser** for the centered logo/search block, with no severe control overlap.
* **Automated DDG regression harness in CI**: a pinned DDG HTML snapshot drives the focus/type/submit/navigate flow headlessly; CI fails on regression.
* **Dependency guardrails in CI**: new package-level cycles fail the build.

---

## Non-Goals (keep the blast radius controlled)

* No full flexbox spec coverage — only the property subset real target pages need (`flex`, `flex-direction`, `flex-wrap`, `flex-basis`, `flex-shrink`, `align-items`, `justify-content`, `order`).
* No CSS Grid beyond an optional fixed-track MVP (P2, only if schedule allows).
* No compositor / layer-tree / raster-thread work; perf items in this milestone are bounded hygiene, not architecture.
* No animation timing engine (`transition`/`transform` parse-and-apply-static only, P2).
* No new threading (DOM/layout/style stays main-thread deterministic).
* No `position: fixed`/`sticky`, scroll containers, or stacking-context work — that "Layouter v2" slice stays in the roadmap for a later milestone (DDG HTML does not need it).

---

## Critical Path (what must land for the North Star)

**Must-have**

* **Flexbox MVP** (`T-LAYOUT-FLEX-1`) plus **real-page alignment coverage** (`T-DDG-LAYOUT-1`).
* **Float `clear` support** (`T-CSS-CLEAR-1`) for legacy page structures.
* **Border/radius longhands + vendor aliases** (`T-CSS-BORDER-COMPAT-1`) for control chrome.
* **DDG CSS compatibility carryover** (`T-DDG-CSS-CORE-2`) — the deltas deferred from M5.
* **DDG snapshot regression harness** (`T-DDG-E2E-1`) wired into CI.
* **Dependency guardrails in CI** (`T-ARCH-GUARD-1`).

**Nice-to-have (if schedule allows)**

* Vendor prefix alias layer (`T-CSS-COMPAT-ALIAS-1`), legacy property slice (`T-CSS-MISC-LEGACY-1`).
* Grid MVP (`T-LAYOUT-GRID-1`), static transition/transform (`T-ANIM-1`).
* Visited link state (`T-HIST-1`), clang-uml diagram cleanup (`T-ARCH-GUARD-2`).

**Perf/memory hygiene track (pull in only if real pages force it)**

* Batch resource updates (`T-PERF-5`), offscreen raster cache (`T-PERF-4`), tab resource eviction (`T-CACHE-1`), DOM virtualization (`T-DOM-1`), DOM budget failure UX (`T-DOM-2`).

---

## Milestone 6 Done When

Milestone 6 is complete when all items below are true:

* Common flex rows/columns render correctly (layout tests pass for the supported property subset).
* DDG HTML homepage shows the logo/search block centered like a reference browser, with no severe control overlap (manual DDG checklist passes — recreate it under `doc/checklists/` since the file referenced by earlier TODOs was never committed).
* CI runs the DDG snapshot harness and fails on typing/submit/navigation or layout regression.
* CI fails deterministically on new package-level dependency cycles.
* No new forbidden dependency edges (Ports & Adapters intact).

---

## Stories

### 6.1 - Flexbox & Layout Compatibility

* **[M6 P1] T-LAYOUT-FLEX-1: Flexbox Layout MVP**; Goal: basic flex layout; Scope: layout engine; Acceptance: common flex rows/columns render; Tests: layout tests.
* **[M6 P1] T-DDG-LAYOUT-1: Flex Alignment Coverage For Real Pages**; Goal: close major DDG homepage positioning gap; Scope: support `flex`, `flex-direction`, `flex-wrap`, `flex-basis`, `flex-shrink`, `align-items`, `justify-content`, `order` with enough fidelity for form/logo centering flows; Acceptance: DDG logo and search form center similarly to reference browser; Tests: layout regression fixture for DDG-like structure.
* **[M6 P1] T-CSS-CLEAR-1: Float Clear Property**; Goal: prevent float/layout overlap in legacy page structures; Scope: parse/apply `clear` and integrate with block flow line placement; Acceptance: blocks that rely on `clear` no longer overlap preceding floated elements; Tests: block layout regressions.
* **[M6 P2] T-LAYOUT-GRID-1: Grid Layout MVP**; Goal: minimal CSS Grid support; Scope: layout engine; Acceptance: fixed-track grids render; Tests: layout tests.

### 6.2 - CSS Compatibility Slice

* **[M6 P1] T-CSS-BORDER-COMPAT-1: Border Longhands And Corner Radius Longhands**; Goal: remove visible control chrome mismatches on real forms; Scope: add `border-top/right/bottom/left-color`, `border-top-left/right/bottom-left/bottom-right-radius`, and vendor aliases (`-moz-`, `-webkit-`) mapped to standard properties; Acceptance: DDG search control corners/borders match expected shape without split seams; Tests: parser/style/paint regressions.
* **[M6 P2] T-CSS-COMPAT-ALIAS-1: Vendor Prefix Alias Layer**; Goal: reduce noisy unsupported-property fallout for legacy CSS; Scope: alias common prefixed properties to supported canonical forms where behavior is equivalent (`-webkit-user-select`, `-moz-appearance`, `-webkit-tap-highlight-color`, etc.) and silently ignore purely cosmetic no-op aliases; Acceptance: warning noise drops on DDG-like pages without behavioral regressions; Tests: parser alias tests.
* **[M6 P2] T-CSS-MISC-LEGACY-1: Legacy Property Compatibility Slice**; Goal: close remaining DDG visual deltas not covered by flex/border work; Scope: targeted support for `text-shadow`, `visibility`, `pointer-events`, and legacy `clip` usage required by DDG assets/icons; Acceptance: DDG search icon/control visuals and hit behavior align with reference browser; Tests: style/layout interaction regressions.
* **[M6 P2] T-ANIM-1: transition + transform (static)**; Goal: parse/apply transforms without timing engine; Scope: style + paint; Acceptance: transform affects paint matrix; Tests: renderer tests.

### 6.3 - DDG Parity + Regression Harness

* **[M6 P1] T-DDG-CSS-CORE-2: DDG CSS Compatibility Carryover**; Goal: finish the remaining DDG visual-compatibility deltas deferred from Milestone 5; Scope: extend CSS/layout compatibility to close search/logo alignment and control-chrome gaps on DDG HTML; Acceptance: DDG homepage matches reference layout for centered logo/search block and no severe control overlap; Tests: DDG manual checklist (`doc/checklists/m5_ddg_css_core.md`) plus targeted regressions.
* **[M6 P1] T-DDG-E2E-1: Real DDG HTML Snapshot Regression Harness**; Goal: validate DDG usability end-to-end against a realistic fixture; Scope: add pinned DDG HTML snapshot fixture, drive focus/type/submit/navigation flow in headless tab/app harness, and assert no overlap/regression around search controls; Acceptance: CI fails if DDG-like typing/submit flow regresses; Tests: engine/app integration tests.

### 6.4 - Architecture Guardrails

* **[M6 P1] T-ARCH-GUARD-1: Dependency Guardrails in CI**; Goal: prevent cycle regressions after refactors; Scope: add automated check for package-level cycles (using clang-uml artifacts or include-graph checks) and fail CI on new violations; Acceptance: CI reports cycle regressions deterministically; Tests: tooling/CI smoke.
* **[M6 P1] T-ARCH-GUARD-2: Clang-UML Diagram Signal Cleanup**; Goal: keep architecture diagrams actionable; Scope: tune config/filters to reduce template/anonymous noise and emit stable focused diagrams for package and engine pipeline views; Acceptance: diagrams highlight project classes/modules with low noise and are documented in dev guide; Tests: docs/tooling.

### 6.5 - Performance & Memory Hygiene (bounded)

* **[M6 P1] T-PERF-5: Batch Resource Updates**; Goal: coalesce resource arrivals into fewer style/layout passes; Scope: ResourceLoader + DocumentPipeline; Acceptance: heavy pages avoid repeated full rebuilds; Tests: engine perf tests.
* **[M6 P1] T-PERF-4: Offscreen Raster Cache + Layer Invalidation**; Goal: repaint only dirty regions; Scope: renderer + engine invalidation; Acceptance: cached layers reused across frames; Tests: renderer perf tests.
* **[M6 P1] T-CACHE-1: Tab Resource Eviction + Rehydrate**; Goal: evict resources/render tree for background tabs and restore on focus; Scope: Tab + ResourceStore; Acceptance: inactive tabs drop memory and reload on activation; Tests: engine tests.
* **[M6 P1] T-DOM-1: Infinite Scroll DOM Virtualization**; Goal: cap live DOM/resources for unbounded feeds; Scope: DOM/layout + resource eviction; Acceptance: long feeds do not grow memory unbounded and can rehydrate when revisiting content; Tests: engine perf tests.
* **[M6 P1] T-DOM-2: DOM Budget Failure UX**; Goal: show a stable error page and recovery action when DOM budget is exceeded; Scope: DocumentPipeline + ResourceLoader; Acceptance: budget failure shows user-facing page instead of blank reset; Tests: engine tests.

### 6.6 - Polish

* **[M6 P1] T-HIST-1: Visited Link State**; Goal: track visited URLs and apply `vlink` colors appropriately; Scope: history store + style resolution; Acceptance: visited anchors render with `vlink`/`:visited` color; Tests: engine/style tests.

---

## Execution Order Checklist

P0: Layout Compatibility (North Star)
- [x] T-LAYOUT-FLEX-1: Flexbox Layout MVP (single-line; wrap + baseline deferred to T-DDG-LAYOUT-1)
- [ ] T-DDG-LAYOUT-1: Flex Alignment Coverage For Real Pages
- [ ] T-CSS-CLEAR-1: Float Clear Property
- [ ] T-CSS-BORDER-COMPAT-1: Border Longhands And Corner Radius Longhands
- [ ] T-DDG-CSS-CORE-2: DDG CSS Compatibility Carryover

P0: Guardrails (North Star)
- [ ] T-DDG-E2E-1: Real DDG HTML Snapshot Regression Harness
- [ ] T-ARCH-GUARD-1: Dependency Guardrails in CI

P1: Compatibility + Hygiene (pull in as needed)
- [ ] T-CSS-COMPAT-ALIAS-1: Vendor Prefix Alias Layer
- [ ] T-CSS-MISC-LEGACY-1: Legacy Property Compatibility Slice
- [ ] T-PERF-5: Batch Resource Updates
- [ ] T-PERF-4: Offscreen Raster Cache + Layer Invalidation
- [ ] T-CACHE-1: Tab Resource Eviction + Rehydrate
- [ ] T-DOM-1: Infinite Scroll DOM Virtualization
- [ ] T-DOM-2: DOM Budget Failure UX
- [ ] T-HIST-1: Visited Link State
- [ ] T-ARCH-GUARD-2: Clang-UML Diagram Signal Cleanup

P2: Only if Schedule Allows
- [ ] T-LAYOUT-GRID-1: Grid Layout MVP
- [ ] T-ANIM-1: transition + transform (static)
