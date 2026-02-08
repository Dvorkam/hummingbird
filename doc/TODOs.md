# TODOs

## Milestone 3 (Navigator)

Milestone complete. See `doc/todo_archive/milestone3_done.md`.

## Milestone 4 (Scripting)

Milestone defined in `doc/milestones/milestone4.md` (stories moved there).

### Additional M4 blockers (DDG HTML)

## Milestone 5 (Layout/Polish)

- [ ] **[M5 P1] T-ARCH-CYCLE-1: Break Engine Document/Script Cycle**; Goal: remove direct package cycle between `engine/document` and `engine/script`; Scope: introduce narrow script-facing document interfaces and invert dependencies; Acceptance: package graph no longer has `document <-> script` cycle; Tests: engine + script tests unchanged.
- [ ] **[M5 P1] T-ARCH-CYCLE-2: Break PlatformApi/Geometry Cycle**; Goal: decouple `core/platform_api` from `layout/geometry`; Scope: move shared rect/point POD types into a neutral core geometry/types module and update interfaces; Acceptance: package graph no longer has `platform_api <-> geometry` cycle; Tests: app/engine/layout tests unchanged.
- [ ] **[M5 P1] T-ARCH-INCLUDE-1: Slim Tab.h and DocumentPipeline.h Includes**; Goal: reduce header fan-out and rebuild impact; Scope: forward declarations + move heavy includes to `.cpp` for `Tab` and `DocumentPipeline`; Acceptance: include graph fan-out for these headers drops materially and no behavior change; Tests: engine tests.
- [ ] **[M5 P1] T-CSS-VIS-1: opacity (paint-only)**; Goal: apply opacity in paint; Scope: Painter; Acceptance: subtree alpha scales paint; Tests: renderer tests.
- [ ] **[M5 P2] T-UI-FORM-1: Form Control Styling Polish**; Goal: native-like input/button visuals (shading, hover, pressed); Scope: renderer + style defaults; Acceptance: inputs/buttons look intentional and stateful; Tests: manual.
- [ ] **[M5 P2] T-ARCH-SPLIT-1: DocumentPipeline Responsibility Split**; Goal: keep pipeline orchestration thin; Scope: extract focused coordinators/services for link hit-test/form submit/script dispatch/resource apply flow; Acceptance: `DocumentPipeline` public API and internals are slimmer with clear boundaries and unchanged behavior; Tests: document pipeline + tab tests.
- [ ] **[M5 P2] T-ARCH-SPLIT-2: ResourceLoader Decomposition**; Goal: prevent growth of a networking god-object; Scope: split request planning, response integration, and policy (fallback/insecure) into dedicated helpers; Acceptance: `ResourceLoader` complexity reduced and behavior preserved; Tests: resource loader + tab tests.
- [ ] **[M5 P3] T-FORM-3: URL-encoded + spaces**; Goal: use application/x-www-form-urlencoded space encoding; Scope: url encoding; Acceptance: spaces become "+"; Tests: core utils tests.
- [ ] **[M5 P3] T-ARCH-CYCLE-3: Break Dom/StyleCompute Cycle**; Goal: remove `core/dom <-> style/compute` mutual dependency; Scope: isolate style-facing DOM access behind read-only adapter/traits interfaces; Acceptance: package graph no longer has `dom <-> compute` cycle; Tests: style + layout + DOM tests.
- [ ] **[M5 P1] T-REF-ENGINE-1: Reshuffle Engine Modules**; Goal: group engine files by domain (document/tab/resources); Scope: Engine folder structure + namespaces; Acceptance: clearer module layout with minimal includes; Tests: existing engine tests.
- [ ] **[M5 P2] T-CSS-BORDER-2: border-radius (paint)**; Goal: round corners; Scope: Painter; Acceptance: rounded rect paint for background/border; Tests: renderer tests.
- [ ] **[M5 P2] T-CSS-DECOR-1: outline + outline-offset**; Goal: draw outlines; Scope: Painter; Acceptance: outline draws outside border with offset; Tests: renderer tests.
- [ ] **[M5 P2] T-CSS-TEXT-1: Text Effects Polish**; Goal: support `text-transform`, `letter-spacing`, `text-indent`, `text-overflow`, `word-wrap`; Scope: style + text layout/painter; Acceptance: long labels elide/wrap closer to author CSS; Tests: renderer + layout tests.
- [ ] **[M5 P1] T-CSS-SEL-2: Child combinator selector (`>`)**; Goal: support direct-child matching; Scope: CssParser + SelectorMatcher; Acceptance: `.parent > .child` matches direct children only; Tests: selector matcher tests.
- [ ] **[M5 P2] T-IMG-1: Animated GIF/WebP Playback**; Goal: play animated frames; Scope: decoder + renderer scheduling; Acceptance: frames render with timing; Tests: image tests.
- [ ] **[M5 P2] T-IMG-2: SVG Image Decode (Raster)**; Goal: rasterize SVG `<img>` sources; Scope: IImageDecoder + SVG library; Acceptance: svg renders to ImageBitmap; Tests: image tests.
- [ ] **[M5 P1] T-PERF-3: Split UI Chrome From Page Render**; Goal: avoid repainting page while editing URL bar; Scope: app render split; Acceptance: URL bar updates without page repaint; Tests: manual.
- [ ] **[M5 P1] T-LAYOUT-INLINE-2: Inline-Block Baseline Alignment**; Goal: inline-block aligns to text baseline by default; Scope: inline layout + line box metrics; Acceptance: inline-block does not appear to “sink” below text; Tests: layout tests.
- [ ] **[M5 P3] T-CSS-CODE-1: Code/Pre Background Defaults**; Goal: default inline code/pre background should not fight page background; Scope: style defaults + computed style; Acceptance: code/pre render transparent unless author CSS sets a background; Tests: style tests.
- [ ] **[M5 P3] T-LIST-1: Ordered List Markers**; Goal: ordered lists show numeric markers instead of bullets; Scope: list marker layout/paint; Acceptance: `<ol>` renders 1., 2., 3.; Tests: layout tests.
- [ ] **[M5 P3] T-TABLE-1: Table Borders for Visibility**; Goal: make table structure visible without author CSS; Scope: style defaults for `table/td/th` or table paint; Acceptance: tables show cell boundaries in demo; Tests: renderer tests.
- [ ] **[M5 P3] T-HTML-SEM-2: Semantic Landmark Roles (A11y)**; Goal: expose `<header/nav/main/section/article/aside/footer>` semantics for accessibility and tooling; Scope: DOM semantics + a11y hooks; Acceptance: semantic tags report correct roles; Tests: DOM/a11y tests.

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
