# TODOs

## Milestone 3 (Navigator)

Milestone complete. See `doc/todo_archive/milestone3_done.md`.

## Milestone 4 (Scripting)

Milestone defined in `doc/milestones/milestone4.md` (stories moved there).

### Additional M4 blockers (DDG HTML)

## Milestone 5 (Layout/Polish)

Milestone defined in `doc/milestones/milestone5.md` (stories moved there).

- [ ] **[M5 P1] T-APP-TABS-REF-1: Extract Tab Orchestration From BrowserApp**; Goal: prevent `BrowserApp` from growing into a god object; Scope: introduce an app-owned `TabController`/`BrowserChrome` that owns `TabManager` + `TabStrip` and handles tab actions/events behind a narrow interface; Acceptance: `BrowserApp` delegates tab actions and tab-strip hit-testing; Tests: existing app tab tests continue to pass.

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
- [ ] **[M6 P1] T-LAYOUT-FLEX-1: Flexbox Layout MVP**; Goal: basic flex layout; Scope: layout engine; Acceptance: common flex rows/columns render; Tests: layout tests.
- [ ] **[M6 P2] T-LAYOUT-GRID-1: Grid Layout MVP**; Goal: minimal CSS Grid support; Scope: layout engine; Acceptance: fixed-track grids render; Tests: layout tests.
- [ ] **[M6 P2] T-ANIM-1: transition + transform (static)**; Goal: parse/apply transforms without timing engine; Scope: style + paint; Acceptance: transform affects paint matrix; Tests: renderer tests.
