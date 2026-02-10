# TODOs

## Milestone 3 (Navigator)

Milestone complete. See `doc/todo_archive/milestone3_done.md`.

## Milestone 4 (Scripting)

Milestone defined in `doc/milestones/milestone4.md` (stories moved there).

### Additional M4 blockers (DDG HTML)

- [x] **[M5 P1] T-CSS-PERCENT-1: Percentage Unit Parsing/Storage**; Goal: stop mis-parsing `%` values as plain numbers; Scope: tokenizer/parser + style value representation for `%` (e.g. `70%`, `100%`, `24%`) without silently degrading to px-like values; Acceptance: `%` values are preserved through parse/apply pipeline and unit tests cover parse of `%` in width/offset properties; Tests: CSS parser/style tests.
- [x] **[M5 P1] T-LAYOUT-PERCENT-1: Percentage Width/Height Resolution**; Goal: resolve `%` against containing block dimensions for core box sizing; Scope: layout metrics + block/replaced sizing for width/height/min/max where applicable; Acceptance: common patterns (`width:70%`, `width:100%`) render proportionally instead of fixed small widths; Tests: layout tests + DDG smoke fixture assertion.
- [x] **[M5 P1] T-LAYOUT-PERCENT-POS-1: Percentage Offsets For Positioned Elements**; Goal: make `top/right/bottom/left` `%` offsets behave consistently for positioned elements; Scope: positioning resolution path for absolute/relative offsets; Acceptance: `top:24%`-style layouts position as expected relative to containing block; Tests: positioning tests.
- [x] **[M5 P1] T-FORM-HIT-1: Input Click Must Not Trigger Submit**; Goal: prevent accidental form submit when user clicks editable text fields; Scope: tighten form-submit hit semantics and add regression around DDG home form; Acceptance: clicking text input focuses caret and does not navigate; submit occurs only on submit control click or Enter; Tests: tab/document form interaction tests.

## Milestone 5 (Layout/Polish)

Milestone complete for `0.5.0` release scope. Archived in `doc/todo_archive/milestone5_done.md`.
Deferred DDG parity follow-ups were moved to Milestone 6 backlog.

## Milestone 6+ (Big Rocks)

- [ ] **[M6 P1] T-ARCH-GUARD-1: Dependency Guardrails in CI**; Goal: prevent cycle regressions after refactors; Scope: add automated check for package-level cycles (using clang-uml artifacts or include-graph checks) and fail CI on new violations; Acceptance: CI reports cycle regressions deterministically; Tests: tooling/CI smoke.
- [ ] **[M6 P1] T-ARCH-GUARD-2: Clang-UML Diagram Signal Cleanup**; Goal: keep architecture diagrams actionable; Scope: tune config/filters to reduce template/anonymous noise and emit stable focused diagrams for package and engine pipeline views; Acceptance: diagrams highlight project classes/modules with low noise and are documented in dev guide; Tests: docs/tooling.
- [ ] **[M6 P1] T-HIST-1: Visited Link State**; Goal: track visited URLs and apply `vlink` colors appropriately; Scope: history store + style resolution; Acceptance: visited anchors render with `vlink`/`:visited` color; Tests: engine/style tests.
- [ ] **[M6 P1] T-PERF-4: Offscreen Raster Cache + Layer Invalidation**; Goal: repaint only dirty regions; Scope: renderer + engine invalidation; Acceptance: cached layers reused across frames; Tests: renderer perf tests.
- [ ] **[M6 P1] T-CACHE-1: Tab Resource Eviction + Rehydrate**; Goal: evict resources/render tree for background tabs and restore on focus; Scope: Tab + ResourceStore; Acceptance: inactive tabs drop memory and reload on activation; Tests: engine tests.
- [ ] **[M6 P1] T-DOM-1: Infinite Scroll DOM Virtualization**; Goal: cap live DOM/resources for unbounded feeds; Scope: DOM/layout + resource eviction; Acceptance: long feeds do not grow memory unbounded and can rehydrate when revisiting content; Tests: engine perf tests.
- [ ] **[M6 P1] T-DOM-2: DOM Budget Failure UX**; Goal: show a stable error page and recovery action when DOM budget is exceeded; Scope: DocumentPipeline + ResourceLoader; Acceptance: budget failure shows user-facing page instead of blank reset; Tests: engine tests.
- [ ] **[M6 P1] T-PERF-5: Batch Resource Updates**; Goal: coalesce resource arrivals into fewer style/layout passes; Scope: ResourceLoader + DocumentPipeline; Acceptance: heavy pages avoid repeated full rebuilds; Tests: engine perf tests.
- [ ] **[M8 P2] T-DOM-CUSTOM-1: Custom Elements Upgrade**; Goal: allow JS `customElements.define()` to upgrade dash-named tags and run lifecycle hooks; Scope: DOM + JS bindings; Acceptance: defined custom element runs constructor/connectedCallback and can attach shadow/DOM; Tests: DOM/JS integration tests.
- [ ] **[M6 P1] T-DDG-CSS-CORE-2: DDG CSS Compatibility Carryover**; Goal: finish the remaining DDG visual-compatibility deltas deferred from Milestone 5; Scope: extend CSS/layout compatibility to close search/logo alignment and control-chrome gaps on DDG HTML; Acceptance: DDG homepage matches reference layout for centered logo/search block and no severe control overlap; Tests: DDG manual checklist (`doc/checklists/m5_ddg_css_core.md`) plus targeted regressions.
- [ ] **[M6 P1] T-DDG-E2E-1: Real DDG HTML Snapshot Regression Harness**; Goal: validate DDG usability end-to-end against a realistic fixture; Scope: add pinned DDG HTML snapshot fixture, drive focus/type/submit/navigation flow in headless tab/app harness, and assert no overlap/regression around search controls; Acceptance: CI fails if DDG-like typing/submit flow regresses; Tests: engine/app integration tests.
- [ ] **[M6 P1] T-LAYOUT-FLEX-1: Flexbox Layout MVP**; Goal: basic flex layout; Scope: layout engine; Acceptance: common flex rows/columns render; Tests: layout tests.
- [ ] **[M6 P1] T-DDG-LAYOUT-1: Flex Alignment Coverage For Real Pages**; Goal: close major DDG homepage positioning gap; Scope: support `flex`, `flex-direction`, `flex-wrap`, `flex-basis`, `flex-shrink`, `align-items`, `justify-content`, `order` with enough fidelity for form/logo centering flows; Acceptance: DDG logo and search form center similarly to reference browser; Tests: layout regression fixture for DDG-like structure.
- [ ] **[M6 P1] T-CSS-CLEAR-1: Float Clear Property**; Goal: prevent float/layout overlap in legacy page structures; Scope: parse/apply `clear` and integrate with block flow line placement; Acceptance: blocks that rely on `clear` no longer overlap preceding floated elements; Tests: block layout regressions.
- [ ] **[M6 P1] T-CSS-BORDER-COMPAT-1: Border Longhands And Corner Radius Longhands**; Goal: remove visible control chrome mismatches on real forms; Scope: add `border-top/right/bottom/left-color`, `border-top-left/right/bottom-left/bottom-right-radius`, and vendor aliases (`-moz-`, `-webkit-`) mapped to standard properties; Acceptance: DDG search control corners/borders match expected shape without split seams; Tests: parser/style/paint regressions.
- [ ] **[M6 P2] T-CSS-COMPAT-ALIAS-1: Vendor Prefix Alias Layer**; Goal: reduce noisy unsupported-property fallout for legacy CSS; Scope: alias common prefixed properties to supported canonical forms where behavior is equivalent (`-webkit-user-select`, `-moz-appearance`, `-webkit-tap-highlight-color`, etc.) and silently ignore purely cosmetic no-op aliases; Acceptance: warning noise drops on DDG-like pages without behavioral regressions; Tests: parser alias tests.
- [ ] **[M6 P2] T-CSS-MISC-LEGACY-1: Legacy Property Compatibility Slice**; Goal: close remaining DDG visual deltas not covered by flex/border work; Scope: targeted support for `text-shadow`, `visibility`, `pointer-events`, and legacy `clip` usage required by DDG assets/icons; Acceptance: DDG search icon/control visuals and hit behavior align with reference browser; Tests: style/layout interaction regressions.
- [ ] **[M6 P2] T-LAYOUT-GRID-1: Grid Layout MVP**; Goal: minimal CSS Grid support; Scope: layout engine; Acceptance: fixed-track grids render; Tests: layout tests.
- [ ] **[M6 P2] T-ANIM-1: transition + transform (static)**; Goal: parse/apply transforms without timing engine; Scope: style + paint; Acceptance: transform affects paint matrix; Tests: renderer tests.
