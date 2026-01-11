## Rendering Performance Roadmap

- **Milestone 3 (Navigator):** Tracked in `doc/milestones/milestone3.md` (Epic 3.9).
- **Milestone 4 (Scripting):** Introduce retained display list to avoid rebuilding paint commands for static content.
- **Milestone 5 (Extensions/UI):** Split UI chrome (URL bar) from page rendering so editing the URL bar doesn't repaint the page.
- **Milestone 6 (Speedster):** Add offscreen raster cache + layer invalidation to repaint only dirty regions.
- **Text Rendering Cache:** Cache `FontSetup` (and optionally text textures) in `SDLGraphicsContext` so per-frame paint avoids reloading fonts and rebuilding text textures.

## Typography Follow-Ups

- **Font Face Mapping:** Expand `ComputedStyle::font_face` beyond the current Roboto-only mapping (proper fallback chain + real monospace fonts).

## Code/Pre Formatting (Stories)

* **Story T-CODE-1: Code Background Blocks**
* **As a** reader,
* **I want** `<code>` (and `pre > code`) backgrounds to render consistently so code blocks remain readable.
* **Acceptance:** Code blocks render with their computed background color across inline and block contexts.

## HTML Tag Coverage

- **Semantic Blocks:** `main`, `section`, `article`, `noscript`
- **Controls:** `button`
- **SVG Core:** `svg`, `g`, `defs`, `path`, `rect`, `circle`, `clippath`

## Legacy HTML Attributes (Stories)

* **Story T-HTML-1: Body Color Attributes**
* **As a** reader of classic HTML pages,
* **I want** `<body bgcolor/text/link/vlink>` to map to background and text/link colors.
* **Acceptance:** Body background + base text/link colors reflect legacy attributes when present.

## HTML Parsing Follow-Ups (Stories)

* **Story T-HTML-2: Decode Named Entities**
* **As a** reader of legacy HTML,
* **I want** named entities like `&mdash;` to decode into their Unicode characters so text renders correctly.
* **Acceptance:** Common named entities (`&mdash;`, `&nbsp;`, `&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`) decode in text nodes.

## CSS Coverage Backlog

- **P2: Selector Coverage:** Add universal selector `*`, descendant combinators, and compound selectors (tag+class/id).
- **P2: Typography + Box Model Extras:** Implement `font-family`, `box-sizing`, and `outline` support (no images/gradients yet).

## CSS/Layout Follow-Ups (Stories)

* **Story T-CSS-1: Length Units (em)**
* **As a** user,
* **I want** CSS `em` lengths to resolve against font size so rules like `width: 10em` apply.
* **Acceptance:** `em` values resolve to pixels for width/height/max/min/margins/padding where supported.

* **Story T-CSS-2: Border Styles Beyond Solid**
* **As a** user,
* **I want** `border-style: outset` (and related styles) to render so classic HTML rules look correct.
* **Acceptance:** `outset` renders with visible borders (can map to solid for MVP).

* **Story T-LAYOUT-1: HR Width/Border Rendering**
* **As a** user,
* **I want** `<hr>` to honor width/height/border styles so separators match their CSS.
* **Acceptance:** RenderRule uses computed width/height and border paint (or equivalent) instead of fixed defaults.

* **Story T-LAYOUT-2: Float Layout (Right/Left)**
* **As a** user,
* **I want** `float: right/left` to position images like validator badges correctly.
* **Acceptance:** Floats affect inline flow and can place an image on the right edge of its container.

## Image Pipeline Follow-Ups (Stories)

* **Story T-IMG-1: Animated GIF/WebP Playback**
* **As a** user,
* **I want** animated GIF/WebP frames to play so pages render as intended.
* **Acceptance:** Decoder exposes frames + timing; renderer schedules frame swaps without blocking the main thread.

* **Story T-IMG-2: SVG Image Decode (Raster)**
* **As a** user,
* **I want** SVG referenced from `<img>` to render as pixels so logos and icons show up.
* **Acceptance:** SVG rasterization is handled behind `IImageDecoder` (via a dedicated SVG library), producing `ImageBitmap` output.

## Table/Layout Follow-Ups

- **Table Cell Block Alignment:** `text-align` only offsets inline runs; add centering/right alignment for block-level children inside table cells (ACME header mismatch).

## Networking Follow-Ups (Stories)

* **Story T-NET-1: TLS Trust Store / Insecure Toggle**
* **As a** user navigating HTTPS sites,
* **I want** curl to trust the system CA bundle (and optionally allow an explicit "insecure" mode for dev) so pages load without manual `-k`.
* **Acceptance:** HTTPS requests succeed with valid cert chains; a debug-only flag can bypass verification when needed.

* **Story T-NET-2: Compressed Response Handling**
* **As a** user visiting modern sites,
* **I want** curl to transparently decode gzip/br/zstd responses so HTML arrives as text.
* **Acceptance:** Enable curl features (brotli/zstd) in vcpkg and verify `Content-Encoding` responses are decoded into plain HTML.

## Branding / Packaging Follow-Ups

- **Windows ICO Sizes:** Add 48/64/128 sizes into `assets/logos/hummingbird.ico` for better Windows scaling.
- **Runtime Icon Layout:** Move runtime PNG/ICO into `assets/icons/` and keep SVGs in `assets/logos/` (update references).

## Engine / App Split Follow-Ups

- Tracked in `doc/milestones/milestone3.md` (Epic 3.1, Epic 3.6, Epic 3.10).

## Refactor Plan (Checklist)

- [x] **Phase 0: Scope + Guardrails**
- [x] Confirm which subsystems are in-scope for the first pass (engine, layout, renderer, platform, app).
- [x] Define constraints (no behavior changes, tests must pass, keep public APIs stable unless agreed).
- [x] Decide review cadence (one slice per commit).
- [x] Subsystem roster (track progress):
- [x] Core
- [x] Html
- [x] Style
- [x] Layout
- [x] Renderer
- [x] Engine
- [x] Platform
- [x] App
- [x] Tests/Utilities

- [x] Phase 0 notes (internal):
- [x] Scope: Core, Html, Style, Layout, Renderer, Engine, Platform, App, Tests/Utilities.
- [x] Constraints: refactor-only; no behavior changes; tests must pass; public APIs stable unless agreed.
- [x] Cadence: one self-contained slice per commit (format/build/tests each slice).

- [x] **Phase 1: Inventory + Baseline**
- [x] Map module responsibilities + public surfaces (top-level summary per folder).
- [x] Capture a dependency sketch (Core → Html/Style/Layout/Renderer → Engine → Platform).
- [x] List known pain points + hotspots from recent work (Tab pipeline, resource store, graphics context).

- [x] Phase 1 notes (internal):
- [x] Module responsibilities summary: Core (arena, DOM, utils, platform_api), Html (tokenizer/parser/tag+attr tables), Style (CSS tokenizer/parser, selector match, UA/defaults), Layout (render tree + layout boxes), Renderer (Painter), Engine (Tab/ResourceStore pipeline), Platform (SDL/Curl adapters, decoders), App (BrowserApp + main), Tests/Utilities (GTest, mocks, headless harness).
- [x] Dependency sketch: Core ← Html/Style/Layout/Renderer; Engine coordinates those; Platform implements Core platform_api; App wires Engine + Platform; Tests depend on module under test + test harness.
- [x] Engine inventory captured (Tab + ResourceStore).
- [x] Tab owns navigation/resource fetch/parse/layout/paint/hit-test; candidate to split into DocumentPipeline + ResourceLoader.
- [x] request_stylesheets/request_images duplicate URL resolve + fetch flow; extract shared helper.
- [x] URL resolution repeated in request_stylesheets/request_images/update_image_resources/build_css_source; centralize helper for consistent keys.
- [x] ResourceStore::request/mark_loading used only by begin_request; consider making private or folding.
- [x] IResourceProvider::load_text used for binary images; consider load_bytes API to clarify intent.
- [x] Image source discovery does extra DOM traversal; consider parser output or shared DOM visitor.
- [x] Platform inventory captured (SDLWindow/GraphicsContext/ImageDecoder, CurlNetwork/StubNetwork, FileResourceProvider).
- [x] CurlNetwork + StubNetwork duplicate thread lifecycle + stop/join code; extract a small worker manager helper.
- [x] Network worker setup + callback patterns duplicated (response init, stop checks); consider shared utility for consistent behavior/logs.
- [x] FileResourceProvider only offers load_text but is used for binary image bytes; add load_bytes or split providers.
- [x] SDLWindow event translation helpers are static in cpp; if this grows, extract to a platform/input translation helper.
- [x] Html inventory captured (Tokenizer + Parser + TagNames/AttributeNames).
- [x] Parser has local to_lower/iequals/find_attribute helpers; consider centralizing shared HTML string utils.
- [x] is_void_element/is_known_element embed tag lists separate from TagNames; centralize tag metadata to avoid drift.
- [x] Tokenizer stores attributes in fixed array of 8; consider dynamic storage or explicit limit handling.
- [x] Style inventory captured (Tokenizer/Parser/SelectorMatcher/StyleEngine/ComputedStyle).
- [x] StyleEngine.cpp houses UA defaults + legacy HTML attribute parsing; consider splitting UA defaults into dedicated module.
- [x] SelectorMatcher only supports tag/class/id; consider centralizing selector parsing/matching table as it grows.
- [x] CSS Parser uses manual property name mapping; consider table-driven map or enum lookup to reduce long if-chain.
- [x] Layout inventory captured (RenderObject + Block/Inline/Text/Image/List/Table + TreeBuilder + InlineLineBuilder).
- [x] BlockBox + RenderListItem duplicate inline layout helpers (metrics, inline runs, alignment); consider shared inline layout utility.
- [x] compute_metrics/insets helpers duplicated across BlockBox/InlineBox/RenderListItem/RenderImage/RenderTable/TextBox.
- [x] TreeBuilder mixes tag routing + style display logic; consider extracting tag→render mapping table.
- [x] Renderer inventory captured (Painter).
- [x] Painter duplicates rect intersection logic already present in Layout/Tab; consider centralizing geometry helpers.
- [x] Painter::paint calls root.paint then also does culled traversal when viewport is set (double paint); consider conditional path or removal if redundant.
- [x] Core inventory captured (utils, arena allocator, DOM, platform_api).
- [x] Url.cpp has its own to_lower/trim/iequals logic separate from Html/CSS helpers; consider shared string utilities.
- [x] ArenaAllocator throws bad_alloc; consider a consistent error path or logging to avoid silent crashes.
- [x] App inventory captured (BrowserApp + main).
- [x] BrowserApp mixes input handling, rendering, and UI state; consider extracting a URLBar component for clarity.
- [x] main.cpp checks graphics context then discards it; consider moving gfx creation into BrowserApp or removing redundant check.
- [x] Tests/Utilities inventory captured (GTest suite + TestGraphicsContext + HeadlessTabHarness).
- [x] TestGraphicsContext duplicates simple geometry/metrics logic; consider shared test utilities folder for harness + graphics mock.
- [x] tests/CMakeLists.txt includes app sources directly; consider a separate test-support target to avoid pulling app into unit suite.

- [x] **Phase 2: Candidate Discovery**
- [x] Scan for duplication (helpers, URL handling, resource logging, render tree traversal).
- [x] Scan for dead/unused code (unused functions, unused headers, obsolete files).
- [x] Flag spaghetti risk (large functions, cross-layer leakage, unbounded ownership).
- [x] Produce a ranked candidate list (quick wins, medium refactors, risky changes).
- [x] Phase 2 candidate list (triage with tags):
- [x] P2-C1: Centralize URL resolution + keying helper used by Tab resource flow. (scope=Engine/Core, risk=low, deps=none, touch=2-3 files)
- [x] P2-C2: Extract shared request helper for stylesheet/image fetch (begin_request + provider + network). (scope=Engine, risk=low, deps=P2-C1, touch=1-2 files)
- [x] P2-C3: Fold ResourceStore::request/mark_loading into begin_request or make private. (scope=Engine, risk=low, deps=none, touch=2 files)
- [x] P2-C4: Split Tab pipeline into DocumentPipeline + ResourceLoader. (scope=Engine, risk=high, deps=P2-C1/P2-C2, touch=3-5 files)
- [x] P2-C5: Replace DOM image traversal with parser output or shared DOM visitor. (scope=Engine/Html, risk=med, deps=none, touch=2-3 files)
- [x] P2-C6: Add load_bytes to IResourceProvider (text vs binary) and update FileResourceProvider + call sites. (scope=Core/Platform/Engine, risk=med, deps=none, touch=4-6 files)
- [x] P2-C7: Extract network worker/thread manager for CurlNetwork + StubNetwork. (scope=Platform, risk=med, deps=none, touch=2-3 files)
- [x] P2-C8: Share Curl/Stub response init + stop checks utility. (scope=Platform, risk=med, deps=P2-C7, touch=2-3 files)
- [x] P2-C9: Extract SDLWindow input translation helpers into platform/input util. (scope=Platform, risk=low, deps=none, touch=2 files)
- [x] P2-C10: Centralize HTML string utils (to_lower/iequals/find_attribute) used in parser. (scope=Html/Core, risk=low, deps=none, touch=2-3 files)
- [x] P2-C11: Centralize tag metadata (known/void tags) to avoid drift with TagNames. (scope=Html, risk=med, deps=none, touch=2 files)
- [x] P2-C12: Replace fixed 8-attr array in HtmlTokenizer with dynamic storage or explicit overflow handling. (scope=Html, risk=med, deps=none, touch=2 files)
- [x] P2-C13: Split UA defaults + legacy attribute parsing into dedicated style module. (scope=Style, risk=med, deps=none, touch=2-3 files)
- [x] P2-C14: Make selector parsing/matching extensible (table-driven or composite selectors). (scope=Style, risk=med, deps=none, touch=2-3 files)
- [x] P2-C15: Replace CSS property name if-chain with table/map lookup. (scope=Style, risk=low, deps=none, touch=1-2 files)
- [ ] (Conditional) If CSS property list grows beyond ~50 entries, replace linear scan with sorted array + binary search in CssParser. (scope=Style, risk=low, deps=none, touch=1 file)
- [x] P2-C16: Deduplicate inline layout helpers between BlockBox and RenderListItem. (scope=Layout, risk=med-high, deps=none, touch=3-4 files)
- [x] P2-C17: Factor shared compute_metrics/insets helpers across layout renderers. (scope=Layout, risk=med, deps=none, touch=4-6 files)
- [x] P2-C18: Extract tag→render mapping table from TreeBuilder. (scope=Layout, risk=low-med, deps=none, touch=2 files)
- [x] P2-C19: Centralize geometry helpers (intersects/point-in-rect) used in Painter/Tab/Layout. (scope=Renderer/Layout/Core, risk=low, deps=none, touch=2-4 files)
- [x] P2-C20: Remove double paint path in Painter (avoid root.paint + culled traverse). (scope=Renderer, risk=med, deps=P2-C19, touch=1-2 files)
- [x] P2-C21: Consolidate string/iequals/to_lower helpers across Core/Html/Style. (scope=Core/Html/Style, risk=low, deps=none, touch=3-5 files)
- [x] P2-C22: Reduce repeated AssetPath resolve + .string churn (cache or helper). (scope=Core/App/Layout/Platform, risk=low, deps=none, touch=2-4 files)
- [x] P2-C23: Align ArenaAllocator error path (log or fail-fast strategy). (scope=Core, risk=low, deps=none, touch=1-2 files)
- [x] P2-C24: Extract URL bar component from BrowserApp (state + input + render). (scope=App, risk=med, deps=none, touch=2-3 files)
- [x] P2-C25: Remove redundant gfx creation in main.cpp (move into BrowserApp or drop). (scope=App, risk=low, deps=none, touch=1-2 files)
- [x] P2-C26: Create shared test utilities module (TestGraphicsContext + harness). (scope=Tests/Utilities, risk=low, deps=none, touch=2-3 files)
- [x] P2-C27: Split tests target to avoid pulling app sources directly. (scope=Tests/Build, risk=med, deps=none, touch=2-3 files)
- [x] P2-C28: Extract render tree traversal helper (visitor/DFS) and reuse in Painter culling, Tab hit-test, and image updates. (scope=Layout/Renderer/Engine, risk=med, deps=none, touch=3-5 files)
- [x] P2-C29: Split Tab::consume_pending_resources into per-type handlers to reduce branching and centralize logs. (scope=Engine, risk=low-med, deps=none, touch=1-2 files)
- [x] P2-C30: Add Element attribute lookup helper (string_view key) to avoid repeated attrs.find + std::string key setup. (scope=Core/Html/Engine, risk=low, deps=none, touch=2-4 files)

- Phase 2 ranking (proposed order): P2-C1, P2-C3, P2-C10, P2-C15, P2-C19, P2-C28, P2-C22, P2-C25, P2-C6, P2-C7, P2-C8, P2-C16, P2-C17, P2-C13, P2-C14, P2-C4, P2-C27, P2-C24.

- [ ] **Phase 3: Refactor Slices**
- [ ] Write a short refactor brief for each slice (goal, scope, acceptance, test impact).
- [ ] Execute one slice at a time with a clean commit.
- [ ] Run format/build/tests per slice (per AGENTS.override.md).
- [ ] Record any follow-up tasks in TODOs.

- [ ] **Phase 4: Cleanup + Verification**
- [ ] Remove dead code identified in Phase 2.
- [ ] Consolidate utilities and update include paths.
- [ ] Update docs/README where behavior or structure changed.
- [ ] Re-run full test suite and note skips.


## Milestone 4 (Scripting milestone, but these are “pre-req polish”)

### **P0 (do first, kills lots of bogus warnings)**

* **Story T-HTML-ROBUST-1: Malformed Tag Handling**

* **As a** parser maintainer,

* **I want** malformed tags like `<>` and empty tag names to be treated as text (with graceful recovery),

* **So that** we don’t create bogus DOM nodes and spam warnings.

* **Acceptance:** Inputs containing `<>`, `< >`, `</>`, `<\n>` don’t create elements; parser continues; warning is deduped.

* **Story T-CSS-ROBUST-1: CSS Declaration Recovery**

* **As a** user visiting modern sites,

* **I want** CSS parsing to recover from malformed/unknown declarations,

* **So that** one bad rule doesn’t poison the whole block (and we don’t “invent” properties like `http`, `ribbon`, `bottom-top`, `e_aG`).

* **Acceptance:** In declaration blocks, only `ident ':' value` becomes a declaration; otherwise skip to `;` or `}`; bogus “properties” disappear from logs.

---

### **P1 (high impact page correctness with low scope risk)**

* **Story T-HTML-SEM-1: Semantic Block Tags Map to BlockBox**

* **As a** reader,

* **I want** semantic container tags to behave like `<div>` by default,

* **So that** modern documents produce a sane block structure.

* **Scope:** `header`, `footer`, `nav`, `main`, `section`, `article`, `aside`, `figure`, `figcaption`, `time`

* **Acceptance:** No “unsupported tag” warnings for these; layout matches `<div>` equivalent.

* **Story T-HTML-CUSTOM-1: Custom Elements as Generic Elements**

* **As a** reader,

* **I want** unknown/custom elements (tag names containing `-`) to parse and render as generic elements,

* **So that** sites using web components (without JS) still lay out.

* **Acceptance:** No warnings for `seznam-*` tags; they produce normal nodes; render as block by default (unless CSS later changes display).

* **Story T-CSS-VAR-1: Custom Properties Storage**

* **As a** user,

* **I want** CSS custom properties (`--*`) to be stored and inherited,

* **So that** stylesheets that rely on variables don’t collapse.

* **Acceptance:** `--*` properties are stored on computed style; inheritance works; no unsupported-property warnings for `--*`.

* **Story T-CSS-VAR-2: Minimal var() Resolution for Colors**

* **As a** user,

* **I want** `var(--x)` to resolve at least for `color` and `background-color`,

* **So that** pages using design tokens are readable.

* **Acceptance:** `color/background-color` accept `var(--x)`; fallback in `var(--x, fallback)` works for these two properties.

* **Story T-CSS-TEXT-1: text-align**

* **As a** user,

* **I want** `text-align` to affect inline content,

* **So that** headers and nav rows align correctly.

* **Acceptance:** left/center/right alignment applies to inline runs (existing table follow-up can extend block child alignment later).

* **Story T-CSS-TEXT-2: white-space (nowrap)**

* **As a** user,

* **I want** `white-space: nowrap` to be respected,

* **So that** nav items and badges don’t wrap into nonsense.

* **Acceptance:** `nowrap` prevents line breaking for inline layout; default behavior unchanged.

* **Story T-CSS-TEXT-3: text-decoration (underline/none)**

* **As a** user,

* **I want** `text-decoration` underline to render for links,

* **So that** pages remain navigable and recognizable.

* **Acceptance:** underline draws for inline text (simple line under baseline); `none` disables.

---

### **P2 (still M4, but after the above)**

* **Story T-FORM-1: Button Element Rendering**

* **As a** reader,

* **I want** `<button>` to render with a basic UA style,

* **So that** controls are visible even without JS.

* **Acceptance:** `<button>` produces a box with padding/border/background; inherits font; participates in layout predictably.

* **Story T-FORM-2: Basic Hit-Test + Click Signal**

* **As a** user,

* **I want** buttons to be clickable (even if action is a stub),

* **So that** we can wire scripting/events later.

* **Acceptance:** click hit-test returns target element; engine logs a click event (no navigation required).

* **Story T-SVG-0: SVG Placeholder Box**

* **As a** user,

* **I want** `<svg>` to occupy space even if we don’t render vector content yet,

* **So that** layouts don’t collapse where icons/logos should be.

* **Acceptance:** `<svg>` renders as placeholder rect; width/height attributes respected; child SVG tags don’t produce warnings.

* **Story T-CSS-TYPO-1: font-family Fallback Chain**

* **As a** reader,

* **I want** `font-family` to select a reasonable fallback chain,

* **So that** typography isn’t Roboto-or-bust.

* **Acceptance:** comma-separated families parsed; generic families (`serif/sans-serif/monospace`) map to actual fonts; first available wins.

* **Story T-CSS-TYPO-2: font-style + font-weight**

* **As a** reader,

* **I want** `font-style` and `font-weight` to affect font selection,

* **So that** emphasis/bold are visible.

* **Acceptance:** normal/italic and weight (at least 400/700) reflected in chosen font face (or best-effort simulation if the font lacks variants).

---

## Milestone 5 (post-scripting start, heavier layout/paint correctness)

### **P1**

* **Story T-CSS-POS-1: position:absolute (basic)**

* **As a** user,

* **I want** `position: absolute` with `top/left/right/bottom` to work,

* **So that** overlays/badges/icons appear where intended.

* **Acceptance:** absolutely positioned elements are out-of-flow and positioned relative to a containing block (MVP: nearest positioned ancestor, fallback viewport).

* **Story T-CSS-VIS-1: opacity (paint-only)**

* **As a** user,

* **I want** `opacity` to affect painting,

* **So that** UI elements using fades look correct.

* **Acceptance:** opacity multiplies paint alpha for element subtree (best-effort if backend lacks stackable alpha).

### **P2**

* **Story T-CSS-BOX-1: box-sizing**

* **As a** developer,

* **I want** `box-sizing: border-box` support,

* **So that** width/height behave predictably for modern UI.

* **Acceptance:** border-box sizing affects layout calculations for width/height.

* **Story T-CSS-BORDER-1: border-radius (paint)**

* **As a** user,

* **I want** `border-radius` to round corners,

* **So that** UI doesn’t look like 1997.

* **Acceptance:** rounded rect paint for background/border (MVP: clip or approximate).

* **Story T-CSS-DECOR-1: outline + outline-offset**

* **As a** user,

* **I want** `outline` to render,

* **So that** focus rings and accessibility visuals appear.

* **Acceptance:** outline draws outside border; outline-offset shifts it.

---

## Milestone 6+ (big rocks; schedule only if you want to expand layout scope)

### **P1**

* **Story T-LAYOUT-FLEX-1: Flexbox Layout MVP**
* **As a** user,
* **I want** basic `display:flex` + `flex-direction` + `justify-content` + `align-items` to work,
* **So that** modern navbars and header layouts render.
* **Acceptance:** common flex rows/columns lay out children correctly (no wrap/ordering required at first).

### **P2**

* **Story T-LAYOUT-GRID-1: Grid Layout MVP**

* **As a** user,

* **I want** minimal CSS Grid support (`grid-template-columns/rows`, `grid-row/column`),

* **So that** structured app-like layouts appear.

* **Acceptance:** fixed-track grids render; auto-placement can be deferred.

* **Story T-ANIM-1: transition + transform (no timing engine)**

* **As a** user,

* **I want** transforms/transitions to parse and apply statically,

* **So that** elements at least appear in their final state (even if not animated yet).

* **Acceptance:** `transform` affects paint transform matrix; `transition*` parses but may be ignored until animation system exists.

---

## “Logging sanity” (optional, but I strongly recommend it)

* **Story T-SUPPORT-REG-1: Supported Feature Registry + Deduped Warnings**
* **As a** developer,
* **I want** a centralized registry for supported HTML tags + CSS properties (and deduped logs),
* **So that** we avoid drift and warning spam.
* **Acceptance:** one table drives “known/supported”; warnings are once-per-(tag/property) per document.
