# TODOs

## Milestone 3 (Navigator)

Milestone complete. See `doc/todo_archive/milestone3_done.md`.

## Milestone 4 (Scripting)

Milestone defined in `doc/milestones/milestone4.md` (stories moved there).

### Additional M4 blockers (DDG HTML)

- [ ] **[M4 P1] T-URL-REL-2: Scheme-Relative URLs (`//host/path`)**; Goal: resolve protocol-relative URLs using the current document scheme; Scope: Url resolution + resource fetch; Acceptance: `//duckduckgo.com/...` loads via https; Tests: core URL tests.
- [ ] **[M4 P1] T-CSS-BG-1: Background Images (MVP)**; Goal: render `background-image` with `background-repeat/position/size`; Scope: style + painter; Acceptance: DDG logo + search button render; Tests: renderer tests.
- [ ] **[M4 P1] T-CSS-POS-1: Positioning Basics (relative/absolute)**; Goal: support `position` + `top/left/right/bottom` + `z-index` ordering for out-of-flow elements; Scope: layout flow; Acceptance: DDG search button overlays input correctly; Tests: layout tests.
- [ ] **[M4 P2] T-CSS-BOX-1: Box-Sizing Support**; Goal: honor `box-sizing` (and vendor-prefixed aliases); Scope: layout sizing; Acceptance: DDG search bar sizing matches CSS; Tests: layout tests.
- [ ] **[M4 P2] T-CSS-LIST-1: List-Style Reset**; Goal: support `list-style` / `list-style-type` / `list-style-position`; Scope: style + list marker; Acceptance: CSS reset can remove bullets; Tests: layout tests.
- [ ] **[M4 P2] T-CSS-TEXT-DECOR-2: text-decoration/underline variants**; Goal: support underline + thickness/offset; Scope: painter; Acceptance: hover/visited underline styling matches CSS; Tests: renderer tests.
- [ ] **[M4 P2] T-CSS-TEXT-DECOR-3: Underline baseline alignment**; Goal: keep underline close to glyphs across all lines; Scope: TextBox + inline layout metrics; Acceptance: no line has an underline noticeably detached from its text; Tests: renderer tests.
- [ ] **[M4 P2] T-LAYOUT-METRICS-1: Centralize Size Resolution**; Goal: consolidate width/height/attrs/intrinsic size resolution into a shared layout/metrics utility; Scope: layout sizing helpers; Acceptance: RenderImage/RenderSvg and future replaced elements use one codepath; Tests: layout tests.

## Milestone 5 (Layout/Polish)

- [ ] **[M5 P1] T-CSS-POS-1: position:absolute (basic)**; Goal: support absolute positioning; Scope: layout flow; Acceptance: out-of-flow elements positioned by top/left/right/bottom; Tests: layout tests.
- [ ] **[M5 P1] T-CSS-VIS-1: opacity (paint-only)**; Goal: apply opacity in paint; Scope: Painter; Acceptance: subtree alpha scales paint; Tests: renderer tests.
- [ ] **[M5 P2] T-CSS-BOX-1: box-sizing**; Goal: support border-box; Scope: layout sizing; Acceptance: border-box affects width/height calc; Tests: layout tests.
- [ ] **[M5 P2] T-UI-FORM-1: Form Control Styling Polish**; Goal: native-like input/button visuals (shading, hover, pressed); Scope: renderer + style defaults; Acceptance: inputs/buttons look intentional and stateful; Tests: manual.
- [ ] **[M5 P3] T-FORM-1: Default GET for empty method**; Goal: treat missing/empty form method as GET; Scope: Engine form submit; Acceptance: method="" submits as GET; Tests: engine tests.
- [ ] **[M5 P3] T-FORM-2: Default submit for empty button type**; Goal: treat empty/invalid button type as submit; Scope: Engine hit-test submit; Acceptance: type="" submits; Tests: engine tests.
- [ ] **[M5 P3] T-FORM-3: URL-encoded + spaces**; Goal: use application/x-www-form-urlencoded space encoding; Scope: url encoding; Acceptance: spaces become "+"; Tests: core utils tests.
- [ ] **[M6 P1] T-HIST-1: Visited Link State**; Goal: track visited URLs and apply `vlink` colors appropriately; Scope: history store + style resolution; Acceptance: visited anchors render with `vlink`/`:visited` color; Tests: engine/style tests.
- [ ] **[M5 P2] T-REF-ENGINE-1: Reshuffle Engine Modules**; Goal: group engine files by domain (document/tab/resources); Scope: Engine folder structure + namespaces; Acceptance: clearer module layout with minimal includes; Tests: existing engine tests.
- [ ] **[M5 P2] T-CSS-BORDER-2: border-radius (paint)**; Goal: round corners; Scope: Painter; Acceptance: rounded rect paint for background/border; Tests: renderer tests.
- [ ] **[M5 P2] T-CSS-DECOR-1: outline + outline-offset**; Goal: draw outlines; Scope: Painter; Acceptance: outline draws outside border with offset; Tests: renderer tests.
- [ ] **[M5 P2] T-CSS-SEL-2: Child combinator selector (`>`)**; Goal: support direct-child matching; Scope: CssParser + SelectorMatcher; Acceptance: `.parent > .child` matches direct children only; Tests: selector matcher tests.
- [ ] **[M5 P2] T-LAYOUT-INLINE-4: Inline Baseline Alignment**; Goal: align inline runs on a shared baseline; Scope: text metrics + inline layout; Acceptance: mixed font sizes/weights align without vertical drift; Tests: layout tests.
- [ ] **[M5 P2] T-IMG-1: Animated GIF/WebP Playback**; Goal: play animated frames; Scope: decoder + renderer scheduling; Acceptance: frames render with timing; Tests: image tests.
- [ ] **[M5 P2] T-IMG-2: SVG Image Decode (Raster)**; Goal: rasterize SVG `<img>` sources; Scope: IImageDecoder + SVG library; Acceptance: svg renders to ImageBitmap; Tests: image tests.
- [ ] **[M5 P2] T-PERF-3: Split UI Chrome From Page Render**; Goal: avoid repainting page while editing URL bar; Scope: app render split; Acceptance: URL bar updates without page repaint; Tests: manual.
- [ ] **[M5 P2] T-CSS-CODE-1: Code/Pre Background Defaults**; Goal: default inline code/pre background should not fight page background; Scope: style defaults + computed style; Acceptance: code/pre render transparent unless author CSS sets a background; Tests: style tests.
- [ ] **[M5 P2] T-LAYOUT-INLINE-2: Inline-Block Baseline Alignment**; Goal: inline-block aligns to text baseline by default; Scope: inline layout + line box metrics; Acceptance: inline-block does not appear to “sink” below text; Tests: layout tests.
- [ ] **[M5 P2] T-LIST-1: Ordered List Markers**; Goal: ordered lists show numeric markers instead of bullets; Scope: list marker layout/paint; Acceptance: `<ol>` renders 1., 2., 3.; Tests: layout tests.
- [ ] **[M5 P2] T-TABLE-1: Table Borders for Visibility**; Goal: make table structure visible without author CSS; Scope: style defaults for `table/td/th` or table paint; Acceptance: tables show cell boundaries in demo; Tests: renderer tests.
- [ ] **[M5 P3] T-HTML-SEM-2: Semantic Landmark Roles (A11y)**; Goal: expose `<header/nav/main/section/article/aside/footer>` semantics for accessibility and tooling; Scope: DOM semantics + a11y hooks; Acceptance: semantic tags report correct roles; Tests: DOM/a11y tests.

## Milestone 6+ (Big Rocks)

- [ ] **[M6 P1] T-PERF-4: Offscreen Raster Cache + Layer Invalidation**; Goal: repaint only dirty regions; Scope: renderer + engine invalidation; Acceptance: cached layers reused across frames; Tests: renderer perf tests.
- [ ] **[M6 P1] T-CACHE-1: Tab Resource Eviction + Rehydrate**; Goal: evict resources/render tree for background tabs and restore on focus; Scope: Tab + ResourceStore; Acceptance: inactive tabs drop memory and reload on activation; Tests: engine tests.
- [ ] **[M6 P1] T-DOM-1: Infinite Scroll DOM Virtualization**; Goal: cap live DOM/resources for unbounded feeds; Scope: DOM/layout + resource eviction; Acceptance: long feeds do not grow memory unbounded and can rehydrate when revisiting content; Tests: engine perf tests.
- [ ] **[M6 P1] T-DOM-2: DOM Budget Failure UX**; Goal: show a stable error page and recovery action when DOM budget is exceeded; Scope: DocumentPipeline + ResourceLoader; Acceptance: budget failure shows user-facing page instead of blank reset; Tests: engine tests.
- [ ] **[M6 P1] T-PERF-5: Batch Resource Updates**; Goal: coalesce resource arrivals into fewer style/layout passes; Scope: ResourceLoader + DocumentPipeline; Acceptance: heavy pages avoid repeated full rebuilds; Tests: engine perf tests.
- [ ] **[M8 P2] T-DOM-CUSTOM-1: Custom Elements Upgrade**; Goal: allow JS `customElements.define()` to upgrade dash-named tags and run lifecycle hooks; Scope: DOM + JS bindings; Acceptance: defined custom element runs constructor/connectedCallback and can attach shadow/DOM; Tests: DOM/JS integration tests.
- [ ] **[M6 P1] T-LAYOUT-FLEX-1: Flexbox Layout MVP**; Goal: basic flex layout; Scope: layout engine; Acceptance: common flex rows/columns render; Tests: layout tests.
- [ ] **[M6 P2] T-LAYOUT-GRID-1: Grid Layout MVP**; Goal: minimal CSS Grid support; Scope: layout engine; Acceptance: fixed-track grids render; Tests: layout tests.
- [ ] **[M6 P2] T-ANIM-1: transition + transform (static)**; Goal: parse/apply transforms without timing engine; Scope: style + paint; Acceptance: transform affects paint matrix; Tests: renderer tests.
