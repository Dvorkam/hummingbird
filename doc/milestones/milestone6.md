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
* **[M6 P2] T-CSS-INHERIT-1: `inherit` Keyword Support**; Goal: honor the CSS-wide `inherit` keyword instead of mis-parsing it (DDG: `.search__input { font-family: inherit }` currently logs "Unsupported font family list 'inherit'" and falls back to Roboto rather than taking the parent's value); Scope: detect an `inherit` declaration value in the apply pipeline and copy the parent's computed value for that property (parent style is already threaded through `apply_properties_to_style`); start with the inherited text/font properties that DDG uses; Acceptance: `font-family: inherit` (and `color: inherit`) resolve to the parent's value with no warning; Tests: style tests. *(Filed while completing T-DDG-CSS-CORE-2.)*
* **[M6 P2] T-CSS-BG-SHORTHAND-SIZE-1: `background` Shorthand `position/size` Syntax**; Goal: honor the `<position> / <size>` form in the `background` shorthand (DDG logo: `background: no-repeat center/100% url(...)`); currently the tokenizer drops `/` and the size value (`100%`) leaks into `background-position`, shifting the DDG logo off-centre and clipping the baked-in "DuckDuckGo" wordmark; Scope: emit a slash token, split the shorthand at `/` so values after it apply to `background-size` (not position); Acceptance: the DDG logo centres correctly and shows the full SVG (duck + wordmark); Tests: parser + a background-position/size regression. *(Filed after T-DDG-CSS-CORE-2: the logo shape was fixed by percentage background-size, but position/wordmark trace to this.)*
* **[M6 P3] T-CSS-CALC-1: `calc()` Length Expressions**; Goal: resolve simple `calc()` length expressions (DDG uses `calc()` in ~10 declarations for sizing); Scope: parse `calc(a +/- b)` with px/%/em terms and evaluate against the resolution reference at apply time; no nested/multiplication support required initially; Acceptance: a `width: calc(100% - 20px)` resolves correctly; Tests: parser/style tests. *(Filed while completing T-DDG-CSS-CORE-2.)*
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
* **[M6 P1] T-SEC-URL-1: Resource/Asset Origin Firewall**; Goal: page-controlled URLs must never reach filesystem APIs (a `//host/...` href probed as a local path is a UNC/SMB request to an attacker-chosen host on Windows — caused the 21s DDG freeze); Scope: restrict the asset-provider probe to internal origins (example.dev / explicit fixture roots), assert URLs are resolved before any provider/filesystem call, reject UNC-shaped paths in AssetPath; Acceptance: real-page URLs (`/x`, `//host/x`, `http(s)://...`) provably never hit the asset loader, regression test covers the UNC shape; Tests: resource loader + asset path tests.
* **[M6 P2] T-PERF-STYLE-1: Selector Match Acceleration**; Goal: style apply must scale to real-world sheets (seznam.cz: 9,652 rules x 4,778 nodes = 6.9s per apply); Scope: bucket rules by rightmost selector key (id/class/tag/universal) and only test candidate buckets per element; keep cascade order stable; Acceptance: seznam-class page styles in well under 1s per apply with identical computed styles on existing tests; Tests: style tests unchanged + perf smoke.
* **[M6 P2] T-HTML-RAWTEXT-1: Script/Style Raw-Text Parsing**; Goal: `<script>`/`<style>` content must be tokenized as raw text, not markup (JS strings currently leak into DOM/link discovery and trigger garbage requests like `https://host/' + fallbackUrl + '`); Scope: HtmlTokenizer rawtext mode until matching end tag; Acceptance: markup-looking strings inside scripts produce no elements/resource requests; Tests: parser tests.
* **[M6 P1] T-FORM-HIT-2: Clicks Outside Submit Controls Must Not Submit**; Goal: on DDG, clicking almost anywhere reloads the page while the submit button's box is small — the click-to-submit routing is too eager somewhere between hit-test and form association; Scope: audit engine click dispatch: only genuine submit controls (or Enter in a field) may trigger form submission, clicks on the form's empty area or other children must not; use T-DEBUG-INSPECT-1 console output to identify what is actually hit; Acceptance: on the DDG fixture, synthetic clicks across the page trigger navigation only on the submit button; Tests: document input controller tests with DDG-like form.
* **[M6 P1] T-CSS-IMPORTANT-1: !important Support**; Goal: honor `!important` in the cascade — the built-in dark-mode extension depends on it (`body * { background-color: ... !important }`) and currently loses every specificity fight against author class rules, producing half-dark pages and artifacts like a lone dark focused control on DDG; Scope: parse the `!important` flag, add an importance tier to cascade resolution (author-important > author-normal, extension layer per M5 origin ordering); Acceptance: dark-mode extension darkens real pages consistently; Tests: parser + cascade tests.
* **[M6 P2] T-CSS-VAR-3: var() For Non-Color Properties**; Goal: resolve custom properties in length/radius contexts (DDG: `border-radius: var(--default-border-radius)`, `max-width: var(--max-content-width)`); Scope: extend var() resolution beyond colors to length-valued properties with fallback support; Acceptance: DDG search box gets its 4px radius and width caps from variables; Tests: style tests.
* **[M6 P2] T-POS-ABS-1: Absolute Centering With Opposing Insets**; Goal: `position:absolute; top:0; bottom:0; margin:auto` (and left/right analog) must center the box (DDG search button vertical centering); Scope: positioning resolution for auto margins with both insets set; Acceptance: DDG magnifier button centers inside the form; Tests: positioning tests.
* **[M6 P2] T-CSS-SIBLING-1: Sibling Combinators (`~`, `+`)**; Goal: support general/adjacent sibling combinators in selector matching (DDG: `.search__input:focus~.search__button { background-color:#5b9e4d }` turns the magnifier green while the input is focused); Scope: CssParser combinator parsing + StyleEngine sibling traversal during matching; Acceptance: focusing the DDG input turns the search button green; Tests: style tests.
* **[M6 P2] T-MEDIA-RESIZE-1: Re-Evaluate Media Conditions On Resize**; Goal: crossing a breakpoint after window resize must restyle; Scope: trigger style re-apply (not just relayout) when viewport size changes across any rule's media bounds; Acceptance: resizing across 864px toggles DDG's conditional rules; Tests: engine tests.
* **[M6 P3] T-FONT-FACE-1: @font-face Web Font Loading**; Goal: load author-declared fonts (DDG magnifier is an icon-font glyph from "ddg-serp-icons"); Scope: parse @font-face src url(), fetch via resource pipeline, register with the font cache behind the platform adapter; Acceptance: DDG icon font renders its glyphs instead of missing-glyph boxes; Tests: style/resource tests + manual.
* **[M6 P2] T-DEBUG-INSPECT-1: Debug Hit-Inspect To Console**; Goal: make F1 debugging actionable on real pages (plain outlines are unlabeled and hard to attribute); Scope: while debug outlines are enabled, clicking an element prints to the console (not the screen, to avoid cluttering the render): tag/id/classes, render object type, absolute rect, and key computed-style fields (display/position/width/height/margins/padding); Acceptance: with F1 active, clicking the DDG search input identifies the element and its geometry in the console; Tests: hit-test/inspect unit test where feasible, otherwise manual checklist.

---

## Execution Order Checklist

P0: Layout Compatibility (North Star)
- [x] T-LAYOUT-FLEX-1: Flexbox Layout MVP (single-line; wrap + baseline deferred to T-DDG-LAYOUT-1)
- [x] T-DDG-LAYOUT-1: Flex Alignment Coverage For Real Pages (flex-wrap + wrap-reverse + baseline)
- [x] T-CSS-CLEAR-1: Float Clear Property
- [x] T-CSS-BORDER-COMPAT-1: Border Longhands And Corner Radius Longhands (per-corner radius + per-side color + vendor aliases)
- [x] T-DDG-CSS-CORE-2: DDG CSS Compatibility Carryover (centered round logo + no overlap; residual deltas filed as their own stories)

P0: Guardrails (North Star)
- [x] T-DDG-E2E-1: Real DDG HTML Snapshot Regression Harness (layout guard + focus/type/submit/navigate flow, in CI)
- [x] T-ARCH-GUARD-1: Dependency Guardrails in CI (package-cycle detection in DependencyFirewall test, runs via ctest on Linux+Windows)

P1: Compatibility + Hygiene (pull in as needed)
- [ ] T-CSS-COMPAT-ALIAS-1: Vendor Prefix Alias Layer
- [ ] T-CSS-MISC-LEGACY-1: Legacy Property Compatibility Slice
- [ ] T-CSS-INHERIT-1: `inherit` Keyword Support
- [x] T-CSS-BG-SHORTHAND-SIZE-1: `background` Shorthand `position/size` Syntax (slash token + shorthand split; DDG logo centers)
- [ ] T-CSS-CALC-1: `calc()` Length Expressions
- [ ] T-PERF-5: Batch Resource Updates
- [ ] T-PERF-4: Offscreen Raster Cache + Layer Invalidation
- [ ] T-CACHE-1: Tab Resource Eviction + Rehydrate
- [ ] T-DOM-1: Infinite Scroll DOM Virtualization
- [ ] T-DOM-2: DOM Budget Failure UX
- [ ] T-SEC-URL-1: Resource/Asset Origin Firewall
- [ ] T-HTML-RAWTEXT-1: Script/Style Raw-Text Parsing
- [ ] T-PERF-STYLE-1: Selector Match Acceleration
- [x] T-FORM-HIT-2: Clicks Outside Submit Controls Must Not Submit (hit-test containment fix)
- [ ] T-CSS-IMPORTANT-1: !important Support
- [ ] T-CSS-SIBLING-1: Sibling Combinators (`~`, `+`)
- [ ] T-CSS-VAR-3: var() For Non-Color Properties
- [x] T-POS-ABS-1: Absolute Centering With Opposing Insets (auto margins between top+bottom / left+right; DDG magnifier centers)
- [ ] T-MEDIA-RESIZE-1: Re-Evaluate Media Conditions On Resize
- [ ] T-FONT-FACE-1: @font-face Web Font Loading
- [ ] T-HIST-1: Visited Link State
- [ ] T-DEBUG-INSPECT-1: Debug Hit-Inspect To Console
- [ ] T-ARCH-GUARD-2: Clang-UML Diagram Signal Cleanup

P2: Only if Schedule Allows
- [ ] T-LAYOUT-GRID-1: Grid Layout MVP
- [ ] T-ANIM-1: transition + transform (static)
