## TODOs

### Milestone 3 (Navigator) Remaining

- [ ] **[M3 P1] T-NET-1: TLS Trust Store / Insecure Toggle**; Goal: trust system CA bundle and allow a debug-only insecure flag; Scope: CurlNetwork TLS config + flag wiring; Acceptance: HTTPS succeeds with valid certs, optional bypass works; Tests: `ctest --preset user-ninja-multi-vcpkglt`.
- [ ] **[M3 P1] T-NET-2: Compressed Response Handling**; Goal: decode gzip/br/zstd responses; Scope: CurlNetwork + vcpkg features; Acceptance: Content-Encoding responses are decoded into plain HTML; Tests: `ctest --preset user-ninja-multi-vcpkglt`.
- [ ] **[M3 P2] T-BRAND-1: Windows ICO Sizes**; Goal: add 48/64/128 sizes to ico; Scope: `assets/logos/hummingbird.ico` (sources now in `assets/logos/`); Acceptance: Windows scaling looks correct at common sizes; Tests: n/a.
- [ ] **[M3 P2] T-BRAND-2: Runtime Icon Layout**; Goal: split runtime PNG/ICO into `assets/icons/` and keep SVGs in `assets/logos/`; Scope: asset moves + references; Acceptance: app loads icons from new paths; Tests: `ctest --preset user-ninja-multi-vcpkglt`.
- [x] **[M3 P2] T-REFAC-4: Consolidate Utilities + Include Paths**; Goal: reduce duplicate helpers and normalize includes; Scope: core/utils + include paths; Acceptance: helpers centralized, no include regressions; Tests: `ctest --preset user-ninja-multi-vcpkglt`.
- [ ] **[M3 P2] T-DOC-1: Update Docs/README After Refactor**; Goal: document current structure and build/run notes; Scope: `README.md` + relevant docs; Acceptance: docs match current code layout and workflows; Tests: n/a.
- [ ] **[M3 P2] T-TEST-1: Full Test Suite + Note Skips**; Goal: run full suite and record skips; Scope: tests + TODO note; Acceptance: skips are documented with reasons; Tests: full suite.

Note: Engine/App split work remains tracked in `doc/milestones/milestone3.md` (Epic 3.1, 3.6, 3.10).

### Milestone 4 (Scripting) Backlog

- [ ] **[M4 P0] T-HTML-ROBUST-1: Malformed Tag Handling**; Goal: treat malformed tags as text with recovery; Scope: HtmlParser; Acceptance: `<>`, `< >`, `</>`, `<\n>` do not create elements; Tests: parser tests.
- [ ] **[M4 P0] T-CSS-ROBUST-1: CSS Declaration Recovery**; Goal: skip malformed declarations safely; Scope: CssParser; Acceptance: only `ident ':' value` becomes a declaration, bad rules are skipped; Tests: CSS parser tests.

- [ ] **[M4 P1] T-HTML-SEM-1: Semantic Block Tags Map to BlockBox**; Goal: map semantic container tags to block layout; Scope: TreeBuilder tag routing; Acceptance: no unsupported-tag warnings, layouts match div; Tests: layout tests.
- [ ] **[M4 P1] T-HTML-CUSTOM-1: Custom Elements as Generic Elements**; Goal: allow dash-named tags; Scope: HtmlParser + TreeBuilder; Acceptance: custom elements parse/render as generic blocks; Tests: parser/layout tests.
- [ ] **[M4 P1] T-HTML-ENT-1: Decode Named Entities**; Goal: decode common HTML entities; Scope: HtmlParser text handling; Acceptance: `&mdash;`, `&nbsp;`, `&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;` render correctly; Tests: parser tests.
- [ ] **[M4 P1] T-HTML-ATTR-1: Body Color Attributes**; Goal: map `bgcolor/text/link/vlink`; Scope: StyleDefaults + legacy attribute mapping; Acceptance: body + link colors reflect attributes; Tests: style tests.
- [ ] **[M4 P1] T-CSS-SEL-1: Selector Coverage**; Goal: add universal selector, descendant combinators, and compound selectors; Scope: CssParser + SelectorMatcher; Acceptance: matching behaves per spec subset; Tests: selector tests.
- [ ] **[M4 P1] T-CSS-TEXT-1: text-align**; Goal: align inline runs; Scope: inline layout; Acceptance: left/center/right align inline content; Tests: layout tests.
- [ ] **[M4 P1] T-CSS-TEXT-2: white-space (nowrap)**; Goal: respect nowrap; Scope: inline layout; Acceptance: nowrap prevents line breaks; Tests: layout tests.
- [ ] **[M4 P1] T-CSS-TEXT-3: text-decoration (underline/none)**; Goal: render underline for links; Scope: Painter + TextBox; Acceptance: underline draws for inline text and can be disabled; Tests: renderer tests.
- [ ] **[M4 P1] T-CSS-LEN-1: Length Units (em)**; Goal: resolve em values from font size; Scope: CssParser + style resolution; Acceptance: em converts for width/height/margins/padding; Tests: style/layout tests.
- [ ] **[M4 P1] T-LAYOUT-HR-1: HR Width/Border Rendering**; Goal: honor computed size/border; Scope: RenderRule; Acceptance: hr uses computed width/height/border; Tests: layout/renderer tests.
- [ ] **[M4 P1] T-LAYOUT-FLOAT-1: Float Layout (Right/Left)**; Goal: support float positioning; Scope: layout flow; Acceptance: float shifts inline flow and positions left/right; Tests: layout tests.
- [ ] **[M4 P1] T-TABLE-ALIGN-1: Table Cell Block Alignment**; Goal: align block children in cells; Scope: RenderTable; Acceptance: center/right align works for block children; Tests: table layout tests.

- [ ] **[M4 P2] T-CODE-1: Code Background Blocks**; Goal: render `<code>` backgrounds consistently; Scope: TextBox/Painter; Acceptance: computed background applies to inline and block code; Tests: renderer tests.
- [ ] **[M4 P2] T-CSS-VAR-1: Custom Properties Storage**; Goal: store/inherit `--*` values; Scope: ComputedStyle; Acceptance: custom props are stored and inherited; Tests: style tests.
- [ ] **[M4 P2] T-CSS-VAR-2: Minimal var() Resolution for Colors**; Goal: resolve var() for color/background-color; Scope: style resolution; Acceptance: var() resolves with fallback; Tests: style tests.
- [ ] **[M4 P2] T-CSS-TYPO-1: font-family Fallback Chain**; Goal: parse font-family lists with fallbacks; Scope: StyleEngine + SDLGraphicsContext mapping; Acceptance: generic families map to real fonts; Tests: style/layout tests.
- [ ] **[M4 P2] T-CSS-TYPO-2: font-style + font-weight**; Goal: apply bold/italic selection; Scope: font selection; Acceptance: weight/style affects chosen face or best-effort; Tests: style/layout tests.
- [ ] **[M4 P2] T-CSS-BORDER-1: Border Styles Beyond Solid**; Goal: support `outset` and related styles; Scope: Painter; Acceptance: non-solid styles render (MVP can map to solid); Tests: renderer tests.
- [ ] **[M4 P2] T-SVG-0: SVG Placeholder Box**; Goal: reserve space for `<svg>`; Scope: TreeBuilder + RenderFactory; Acceptance: svg renders placeholder rect and respects width/height; Tests: layout tests.
- [ ] **[M4 P2] T-FORM-1: Button Element Rendering**; Goal: basic `<button>` UA style; Scope: TreeBuilder + StyleDefaults; Acceptance: button has padding/border/background and participates in layout; Tests: layout tests.
- [ ] **[M4 P2] T-FORM-2: Basic Hit-Test + Click Signal**; Goal: click hit-test returns target element; Scope: hit testing + logging; Acceptance: click logs target (no action yet); Tests: engine tests.
- [ ] **[M4 P2] T-FONT-1: Monospace Font Selection**; Goal: pick real monospace fonts when requested; Scope: TextBox + font mapping; Acceptance: monospace uses actual mono face; Tests: layout tests.
- [ ] **[M4 P2] T-SUPPORT-REG-1: Supported Feature Registry + Deduped Warnings**; Goal: centralize supported tags/properties and dedupe warnings; Scope: Html/Css support tables + logging; Acceptance: warnings are once-per-(tag/property) per doc; Tests: parser tests.
- [ ] **[M4 P2] T-PERF-1: Retained Display List (Paint Cache)**; Goal: avoid rebuilding paint commands for static content; Scope: renderer/engine; Acceptance: unchanged output with fewer rebuilds; Tests: renderer tests.
- [ ] **[M4 P2] T-PERF-2: Text Rendering Cache**; Goal: cache FontSetup and optionally text textures; Scope: SDLGraphicsContext; Acceptance: repeated paints avoid font reloads; Tests: renderer tests.

### Milestone 5 (Layout/Polish)

- [ ] **[M5 P1] T-CSS-POS-1: position:absolute (basic)**; Goal: support absolute positioning; Scope: layout flow; Acceptance: out-of-flow elements positioned by top/left/right/bottom; Tests: layout tests.
- [ ] **[M5 P1] T-CSS-VIS-1: opacity (paint-only)**; Goal: apply opacity in paint; Scope: Painter; Acceptance: subtree alpha scales paint; Tests: renderer tests.

- [ ] **[M5 P2] T-CSS-BOX-1: box-sizing**; Goal: support border-box; Scope: layout sizing; Acceptance: border-box affects width/height calc; Tests: layout tests.
- [ ] **[M5 P2] T-CSS-BORDER-2: border-radius (paint)**; Goal: round corners; Scope: Painter; Acceptance: rounded rect paint for background/border; Tests: renderer tests.
- [ ] **[M5 P2] T-CSS-DECOR-1: outline + outline-offset**; Goal: draw outlines; Scope: Painter; Acceptance: outline draws outside border with offset; Tests: renderer tests.
- [ ] **[M5 P2] T-IMG-1: Animated GIF/WebP Playback**; Goal: play animated frames; Scope: decoder + renderer scheduling; Acceptance: frames render with timing; Tests: image tests.
- [ ] **[M5 P2] T-IMG-2: SVG Image Decode (Raster)**; Goal: rasterize SVG `<img>` sources; Scope: IImageDecoder + SVG library; Acceptance: svg renders to ImageBitmap; Tests: image tests.
- [ ] **[M5 P2] T-PERF-3: Split UI Chrome From Page Render**; Goal: avoid repainting page while editing URL bar; Scope: app render split; Acceptance: URL bar updates without page repaint; Tests: manual.

### Milestone 6+ (Big Rocks)

- [ ] **[M6 P1] T-PERF-4: Offscreen Raster Cache + Layer Invalidation**; Goal: repaint only dirty regions; Scope: renderer + engine invalidation; Acceptance: cached layers reused across frames; Tests: renderer perf tests.
- [ ] **[M6 P1] T-LAYOUT-FLEX-1: Flexbox Layout MVP**; Goal: basic flex layout; Scope: layout engine; Acceptance: common flex rows/columns render; Tests: layout tests.
- [ ] **[M6 P2] T-LAYOUT-GRID-1: Grid Layout MVP**; Goal: minimal CSS Grid support; Scope: layout engine; Acceptance: fixed-track grids render; Tests: layout tests.
- [ ] **[M6 P2] T-ANIM-1: transition + transform (static)**; Goal: parse/apply transforms without timing engine; Scope: style + paint; Acceptance: transform affects paint matrix; Tests: renderer tests.
