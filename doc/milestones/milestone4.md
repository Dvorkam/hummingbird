## Milestone 4 North Star Deliverable

**DuckDuckGo HTML mode**:

* You can **type in the search box**, and **press Enter / click Search**, and it navigates to results (even if results look ugly).
* Along the way: the count of “unsupported tag/property” warnings for that flow starts trending down (form/input/button + a handful of CSS bits).

Plus a separate **tiny internal JS demo page** (e.g., `https://example.dev/js`) proving:

* QuickJS runs,
* `onclick` works,
* JS can do one visible DOM mutation (e.g., button click changes a text node or toggles a class).

---

## Non-Goals (keep the blast radius contained)

* No full JS compatibility, no modern site expectations (no frameworks).
* No async JS APIs (fetch/XHR/timers) yet.
* No SVG (unless DDG HTML unexpectedly forces it; otherwise keep SVG as Milestone 5+ backlog).
* No new threading (DOM/layout stays main thread, as per your constraints already enforced in Milestone 3 ).

---

## DDG HTML Critical Path (what must land for the North Star)

The story list below is intentionally long, but the DDG HTML deliverable only depends on a subset. The critical path is:

**Must-have for DDG HTML search flow**

* **Controls + navigation:** Epic 4.1 (form/input/button rendering + focus/edit + GET submit → `Tab::Navigate`).
* **Selector basics:** `T-CSS-SEL-1` so DDG’s CSS matches (universal selector, descendant combinator, compound selectors).
* **Text readability:** `T-HTML-ENT-1` (entities), `T-CSS-TEXT-3` (underline), and at least minimal `T-CSS-TYPO-1` (font-family mapping).
* **Robustness (if it blocks DDG):** `T-HTML-ROBUST-1` and `T-CSS-ROBUST-1` (avoid fatal parse failures on real-world markup/css).

**Nice-to-have for DDG HTML (pull in only if needed)**

* `T-CSS-TEXT-1` (text-align), `T-CSS-TEXT-2` (nowrap), `T-CSS-LEN-1` (em units), `T-HTML-ATTR-1` (legacy body/link colors).
* `T-SUPPORT-REG-1` to make “unsupported warnings trending down” measurable and non-spammy.

**Explicitly not required for the North Star**

* `T-CSS-VAR-1/2` (custom properties), `T-PERF-1` (retained display list), `T-SVG-0` (SVG), `T-LAYOUT-FLOAT-1`, `T-TABLE-ALIGN-1`.

## Epic 4.1 — Form Controls MVP (DDG HTML search without requiring JS)

This is the “make DDG actually usable” epic. Even if DDG’s HTML version doesn’t need JS, it absolutely needs **forms + input + button behavior**.

### Stories

* **Story 4.1.1: Control Rendering (Form/Input/Button)**
* **Goal:** Render `<form>`, `<input>`, and `<button>` as visible elements with basic UA styling.
* **Scope:** Parsing/routing in HTML→render-tree, minimal computed style defaults for controls, basic box rendering.
* **Likely files:** `src/html/*`, `src/layout/*`, `src/style/*`, `src/renderer/*`.
* **Acceptance:** DDG HTML page shows an obvious input box and a clickable button (even if not pretty).
* **Tests:** layout/render integration tests for control boxes.

* **Story 4.1.2: Input Focus + Text Editing MVP**
* **Goal:** Make `<input type=text>` accept text, show a caret, and keep the value stable across frames.
* **Scope:** focus state, text editing (insert/backspace/delete), basic cursor movement (left/right optional).
* **Likely files:** `src/app/*` (event routing), `src/engine/*` (hit-test→focus), `src/layout/*` (paint caret/value).
* **Acceptance:** Click input → type characters → they appear in the input; backspace deletes; focus is obvious.
* **Tests:** headless harness tests for “value string updates” + “focus changes on click”.

* **Story 4.1.3: Form Submit (GET) → Navigate**
* **Goal:** Submitting a form generates a URL with query string and navigates via `Tab::Navigate(...)`.
* **Scope:** `<form action method=get>`, input `name=value`, URL encoding basics, Enter submits nearest form.
* **Likely files:** `src/core/utils/Url.*`, `src/engine/*`, `src/app/*`.
* **Acceptance:** DDG search: type query + Enter navigates to results.
* **Tests:** engine tests for query encoding + navigation URL generation.

* **Story 4.1.4: Button Click Submits Nearest Form**
* **Goal:** Clicking `<button>` triggers form submit (same behavior as Enter).
* **Scope:** hit-test → dispatch click → form association rules (nearest ancestor form).
* **Likely files:** `src/engine/*`, `src/layout/*` (hit-test), `src/app/*`.
* **Acceptance:** DDG search: clicking Search navigates to results.
* **Tests:** engine tests for “click button submits form”.

**Scope**

* DOM/HTML: recognize + render these tags with sane defaults:

  * `<form>`, `<input>`, `<button>`, plus common structural wrappers seen around them (`<header>`, `<main>`, `<section>` as “block-ish divs”).
* Layout: “replaced element” style box for `<input>`/`<button>`:

  * fixed-ish intrinsic height, min width, padding, border.
* Interaction:

  * focus handling for input
  * text editing (basic: insert/backspace, arrows optional but nice)
  * Enter submits the nearest form
  * button click submits the form
* Navigation:

  * form submit generates URL with query string (GET only for MVP), triggers `Tab::Navigate(...)`.

**Acceptance**

* You can execute a search end-to-end on DDG HTML:

  * click input → type query → click button OR press Enter → results page loads.

This leverages the Milestone 3 engine shell and navigation pipeline you already have .

---

## Epic 4.2 — QuickJS Integration (engine-owned, deterministic, and modular)

This is the “The Brain” part, but keep it intentionally tiny and safe.

**Design rules (constitution-aligned)**

* QuickJS lives behind a **Core interface** like `IScriptEngine` (implemented in Platform or a dedicated “third_party adapter” layer), so **Core/Html/Layout never include QuickJS headers** (same Ports & Adapters firewall you’ve been enforcing ).
* No exceptions: errors are returned as `std::optional`/status structs.
* All script execution happens on the **main thread** (consistent with your “single-threaded DOM/layout” rule ).

**MVP bindings**

* `console.log(...)` → your logger.
* `document.getElementById(...)`
* `element.textContent = "..."`
* `element.setAttribute(name,value)` (optional)
* `element.addEventListener("click", fn)` and/or `onclick`

**Events MVP**

* Wire renderer hit-test → DOM node → dispatch `"click"` into JS if handler exists.
* Optional: `"load"` fired once after document pipeline is ready.

**Acceptance**

* Your internal demo page proves:

  * JS runs,
  * clicking a button triggers JS,
  * a DOM mutation triggers **style/layout/paint invalidation** exactly once (not a per-frame rebuild).

This aligns with the roadmap’s “onclick/onload + DOM modify triggers re-layout” goal .

### Stories

* **Story 4.2.1: Script Engine Port (`IScriptEngine`)**
* **Goal:** Introduce a Core-facing interface for scripting so Core/Html/Layout stay QuickJS-free.
* **Scope:** interface definitions, factories, stubs/no-op implementation.
* **Likely files:** `src/core/platform_api/*`, `src/platform/*`.
* **Acceptance:** Engine can construct a script engine object behind an interface without linking QuickJS in Core libs.
* **Tests:** compile/link + basic unit test with stub.

* **Story 4.2.2: QuickJS Backend (Eval + Error Reporting)**
* **Goal:** Implement a QuickJS-backed `IScriptEngine` with safe error reporting and no exceptions.
* **Scope:** embed QuickJS, `eval(script)`, capture/return errors as status.
* **Likely files:** `src/platform/*` (or dedicated adapter folder), build/vcpkg deps.
* **Acceptance:** Internal page can run a trivial script and logs show errors cleanly.
* **Tests:** unit tests for eval success/failure.

* **Story 4.2.3: Minimal DOM Bindings (read + mutate)**
* **Goal:** Bind `document.getElementById`, `element.textContent=...`, and optionally `setAttribute`.
* **Scope:** object identity mapping between JS objects and DOM nodes; lifetime rules documented.
* **Likely files:** `src/core/dom/*`, `src/engine/*`, script adapter files.
* **Acceptance:** Clicking button updates a text node deterministically.
* **Tests:** headless harness tests: JS mutation triggers a single invalidation.

* **Story 4.2.4: Click + Load Event Dispatch**
* **Goal:** Dispatch `click` and `load` into JS.
* **Scope:** renderer hit-test result → DOM node → event dispatch; `load` once per document ready.
* **Likely files:** `src/engine/*`, `src/layout/*`, `src/app/*`.
* **Acceptance:** `onclick` triggers; `load` fires once after initial build.
* **Tests:** engine tests for event routing order.

* **Story 4.2.5: Internal Demo Page (`example.dev/js`)**
* **Goal:** Provide a deterministic test page for JS integration.
* **Scope:** stub document body, embedded scripts, small UI.
* **Likely files:** `src/platform/StubNetwork.*` (or resource fixtures), tests.
* **Acceptance:** demo page reliably shows a JS-driven change.
* **Tests:** smoke/headless regression test for demo flow.

---

## Epic 4.3 — “Unsupported HTML/CSS Warnings Trend Down” (targeted coverage)

Don’t try to “support the web.” Do targeted reductions that directly help:

1. DDG HTML flow
2. common “brutalist” pages you already use as regressions

**HTML tags (priority)**

* Controls: `form`, `input`, `button` (mandatory for your stated MVP)
* Semantic blocks: `header`, `main`, `section`, `article` as simple block containers (cheap wins)
* Defer SVG (`svg`, `path`, etc.) unless a target page forces it (it’s explicitly called out as backlog elsewhere) .

**CSS (priority)**
Focus only on what helps forms/readability:

* Selector upgrades you already identified as backlog: universal `*`, descendant combinator, and compound selectors (tag+class/id) .
* Typography essentials: `font-family` (even if it’s just mapping a few names to bundled fonts) .
* Box model bits that impact controls: padding/border/background-color (most of this exists in your Milestone 2 backlog direction) .

**Acceptance**

* The DDG HTML homepage renders with:

  * visible input box
  * visible button
  * reasonable spacing (not perfect, but “obviously usable”)
* Your unsupported-tag/property log volume for that page is measurably lower than baseline.

### Stories

* **Story 4.3.1: Supported Feature Registry + Deduped Warnings (`T-SUPPORT-REG-1`)**
* **Goal:** Centralize what is supported and avoid log spam.
* **Scope:** registry tables for HTML tags + CSS properties; warn once per doc.
* **Likely files:** `src/html/*`, `src/style/*`, `src/core/utils/Log.*`.
* **Acceptance:** Unsupported warnings for DDG appear once per unique tag/property.
* **Tests:** parser/style tests asserting dedupe behavior.

* **Story 4.3.2: Selector Coverage (`T-CSS-SEL-1`)**
* **Goal:** Ensure DDG CSS actually matches.
* **Scope:** universal selector `*`, descendant combinator, compound selectors (tag+class/id).
* **Likely files:** `src/style/CssParser.*`, `src/style/SelectorMatcher.*`, tests.
* **Acceptance:** DDG-like selectors match and style visibly changes.
* **Tests:** selector matcher tests.

* **Story 4.3.3: Decode Named Entities (`T-HTML-ENT-1`)**
* **Goal:** Improve readability for HTML-mode sites.
* **Scope:** HTML text decoding for a small whitelist of named entities.
* **Likely files:** `src/html/*`, tests.
* **Acceptance:** `&mdash;`/`&nbsp;` render correctly on DDG and existing regression pages.
* **Tests:** parser tests for entity decoding.

* **Story 4.3.4: Robust Parsing (HTML/CSS) (`T-HTML-ROBUST-1`, `T-CSS-ROBUST-1`)**
* **Goal:** Avoid hard failures on real-world markup/css.
* **Scope:** malformed tag recovery + CSS declaration recovery.
* **Likely files:** `src/html/*`, `src/style/*`, tests.
* **Acceptance:** DDG HTML loads without parser aborts on malformed markup; CSS skips malformed declarations safely.
* **Tests:** parser tests + css parser tests.

* **Story 4.3.5: Text Readability (`T-CSS-TEXT-1/2/3`, `T-CSS-LEN-1`)**
* **Goal:** Make pages readable enough to use.
* **Scope:** `text-align`, `white-space: nowrap`, link underlines, and `em` length handling.
* **Likely files:** `src/style/*`, `src/layout/*`, `src/renderer/*`, tests.
* **Acceptance:** DDG results page is legible; link affordance is visible; common spacing units behave.
* **Tests:** style/layout tests.

---

## Epic 4.4 — Fonts & Czech Characters (two-font demo, not a full font system)

You already have typography follow-ups noted (expand beyond Roboto-only) . For Milestone 4, keep it pragmatic:

**Scope**

* Bundle **two fonts**:

  * a readable sans (e.g., Roboto/Noto Sans)
  * a monospace (for `<code>`/`pre`)
* Implement a minimal `font-family` mapping:

  * recognize a short list of common names (`system-ui`, `sans-serif`, `serif`, `monospace`, `Roboto`, `Noto Sans`)
  * pick the best available bundled font with decent Latin Extended coverage (to show Czech working)
* Ensure text measurement + draw use the selected font consistently.

**Note**
If your renderer-side font/text caching work is already done (it appears tracked as “Milestone 4 Done” items like FontSetup cache / texture caches) , then this epic becomes mostly **style plumbing + font selection**, not performance work.

**Acceptance**

* A page that requests a specific font (or `sans-serif`) actually changes appearance.
* Czech text renders with correct glyphs (no missing boxes).

### Stories

* **Story 4.4.1: font-family Mapping (`T-CSS-TYPO-1`)**
* **Goal:** Respect common font-family requests with a small mapping table.
* **Scope:** parse `font-family` lists, map generic families and a few known names to bundled fonts.
* **Likely files:** `src/style/*`, `src/layout/*`, `src/platform/*`.
* **Acceptance:** Pages that request `sans-serif`/`monospace` actually change appearance.
* **Tests:** style/layout tests verifying selected face.

* **Story 4.4.2: Monospace Selection (`T-FONT-1`)**
* **Goal:** Ensure `<code>`/`pre` render with real monospace.
* **Scope:** font selection plumbing and default mappings.
* **Likely files:** `src/style/*`, `src/layout/*`, `src/platform/*`.
* **Acceptance:** code blocks look monospace and measure consistently.
* **Tests:** layout tests for monospace metrics.

* **Story 4.4.3: Bold/Italic Selection (`T-CSS-TYPO-2`)**
* **Goal:** Make bold/italic use the best available face.
* **Scope:** `font-weight`/`font-style` mapping; best-effort fallbacks when faces are missing.
* **Likely files:** `src/style/*`, `src/platform/*`.
* **Acceptance:** headings/strong/em are visually differentiated in a predictable way.
* **Tests:** style/layout tests.

---

## Epic 4.5 — Performance hygiene (only if it blocks DDG)

Your performance roadmap mentions retained display list + avoiding rebuilds , but I’d only pull this into Milestone 4 if:

* input typing causes full re-layout of the entire page every keystroke, or
* button hover/focus invalidates everything.

Given you already have caching-oriented items marked done , the pragmatic Milestone 4 performance goal is:

**“Control invalidation is localized.”**
Typing in an `<input>` should repaint (and maybe re-layout) the control, not rebuild the whole document.

---

## Milestone 4 Story List (moved from TODO)

These are the Milestone 4 stories pulled from `doc/TODOs.md` to keep everything in one place. Items marked `[x]`
are already completed.

### P0

- [x] **[M4 P0] T-HTML-ROBUST-1: Malformed Tag Handling**; Goal: treat malformed tags as text with recovery; Scope: HtmlParser; Acceptance: `<>`, `< >`, `</>`, `<\n>` do not create elements; Tests: parser tests.
- [x] **[M4 P0] T-CSS-ROBUST-1: CSS Declaration Recovery**; Goal: skip malformed declarations safely; Scope: CssParser; Acceptance: only `ident ':' value` becomes a declaration, bad rules are skipped; Tests: CSS parser tests.

### P1

- [x] **[M4 P1] T-HTML-SEM-1: Semantic Block Tags Map to BlockBox**; Goal: map semantic container tags to block layout; Scope: TreeBuilder tag routing; Acceptance: no unsupported-tag warnings, layouts match div; Tests: layout tests.
- [x] **[M4 P1] T-HTML-CUSTOM-1: Custom Elements as Generic Elements**; Goal: allow dash-named tags; Scope: HtmlParser + TreeBuilder; Acceptance: custom elements parse/render as generic blocks; Tests: parser/layout tests.
- [x] **[M4 P1] T-HTML-ENT-1: Decode Named Entities**; Goal: decode common HTML entities; Scope: HtmlParser text handling; Acceptance: `&mdash;`, `&nbsp;`, `&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;` render correctly; Tests: parser tests.
- [x] **[M4 P1] T-HTML-ATTR-1: Body Color Attributes**; Goal: map `bgcolor/text/link/vlink`; Scope: StyleDefaults + legacy attribute mapping; Acceptance: body + link colors reflect attributes; Tests: style tests.
- [x] **[M4 P1] T-CSS-SEL-1: Selector Coverage**; Goal: add universal selector, descendant combinators, and compound selectors; Scope: CssParser + SelectorMatcher; Acceptance: matching behaves per spec subset; Tests: selector tests.
- [x] **[M4 P1] T-CSS-TEXT-1: text-align**; Goal: align inline runs; Scope: inline layout; Acceptance: left/center/right align inline content; Tests: layout tests.
- [x] **[M4 P1] T-CSS-TEXT-2: white-space (nowrap)**; Goal: respect nowrap; Scope: inline layout; Acceptance: nowrap prevents line breaks; Tests: layout tests.
- [x] **[M4 P1] T-CSS-TEXT-3: text-decoration (underline/none)**; Goal: render underline for links; Scope: Painter + TextBox; Acceptance: underline draws for inline text and can be disabled; Tests: renderer tests.
- [x] **[M4 P1] T-CSS-LEN-1: Length Units (em)**; Goal: resolve em values from font size; Scope: CssParser + style resolution; Acceptance: em converts for width/height/margins/padding; Tests: style/layout tests.
- [x] **[M4 P1] T-LAYOUT-HR-1: HR Width/Border Rendering**; Goal: honor computed size/border; Scope: RenderRule; Acceptance: hr uses computed width/height/border; Tests: layout/renderer tests.
- [ ] **[M4 P1] T-LAYOUT-FLOAT-1: Float Layout (Right/Left)**; Goal: support float positioning; Scope: layout flow; Acceptance: float shifts inline flow and positions left/right; Tests: layout tests.
- [ ] **[M4 P1] T-TABLE-ALIGN-1: Table Cell Block Alignment**; Goal: align block children in cells; Scope: RenderTable; Acceptance: center/right align works for block children; Tests: table layout tests.

### P2

- [ ] **[M4 P2] T-CODE-1: Code Background Blocks**; Goal: render `<code>` backgrounds consistently; Scope: TextBox/Painter; Acceptance: computed background applies to inline and block code; Tests: renderer tests.
- [ ] **[M4 P2] T-CSS-VAR-1: Custom Properties Storage**; Goal: store/inherit `--*` values; Scope: ComputedStyle; Acceptance: custom props are stored and inherited; Tests: style tests.
- [ ] **[M4 P2] T-CSS-VAR-2: Minimal var() Resolution for Colors**; Goal: resolve var() for color/background-color; Scope: style resolution; Acceptance: var() resolves with fallback; Tests: style tests.
- [x] **[M4 P2] T-CSS-TYPO-1: font-family Fallback Chain**; Goal: parse font-family lists with fallbacks; Scope: StyleEngine + SDLGraphicsContext mapping; Acceptance: generic families map to real fonts; Tests: style/layout tests.
- [x] **[M4 P2] T-CSS-TYPO-2: font-style + font-weight**; Goal: apply bold/italic selection; Scope: font selection; Acceptance: weight/style affects chosen face or best-effort; Tests: style/layout tests.
- [ ] **[M4 P2] T-CSS-BORDER-1: Border Styles Beyond Solid**; Goal: support `outset` and related styles; Scope: Painter; Acceptance: non-solid styles render (MVP can map to solid); Tests: renderer tests.
- [ ] **[M4 P2] T-SVG-0: SVG Placeholder Box**; Goal: reserve space for `<svg>`; Scope: TreeBuilder + RenderFactory; Acceptance: svg renders placeholder rect and respects width/height; Tests: layout tests.
- [ ] **[M4 P2] T-FORM-1: Button Element Rendering**; Goal: basic `<button>` UA style; Scope: TreeBuilder + StyleDefaults; Acceptance: button has padding/border/background and participates in layout; Tests: layout tests.
- [ ] **[M4 P2] T-FORM-2: Basic Hit-Test + Click Signal**; Goal: click hit-test returns target element; Scope: hit testing + logging; Acceptance: click logs target (no action yet); Tests: engine tests.
- [x] **[M4 P2] T-FONT-1: Monospace Font Selection**; Goal: pick real monospace fonts when requested; Scope: TextBox + font mapping; Acceptance: monospace uses actual mono face; Tests: layout tests.
- [ ] **[M4 P2] T-SUPPORT-REG-1: Supported Feature Registry + Deduped Warnings**; Goal: centralize supported tags/properties and dedupe warnings; Scope: Html/Css support tables + logging; Acceptance: warnings are once-per-(tag/property) per doc; Tests: parser tests.
- [ ] **[M4 P2] T-PERF-1: Retained Display List (Paint Cache)**; Goal: avoid rebuilding paint commands for static content; Scope: renderer/engine; Acceptance: unchanged output with fewer rebuilds; Tests: renderer tests.
- [x] **[M4 P2] T-RENDER-1: Font Setup Cache**; Goal: cache Blend2D font setup per (path,size); Scope: SDLGraphicsContext; Acceptance: draw/measure no longer reloads fonts per call; Tests: renderer tests.
- [x] **[M4 P2] T-RENDER-2: Premeasured Text Draw**; Goal: avoid re-measuring text inside draw calls; Scope: TextBox + SDLGraphicsContext; Acceptance: draw path uses precomputed metrics; Tests: renderer tests.
- [x] **[M4 P2] T-RENDER-3: Text Texture Cache (LRU)**; Goal: reuse SDL textures for repeated strings; Scope: SDLGraphicsContext; Acceptance: text-heavy pages reuse cached textures; Tests: renderer perf tests.
- [x] **[M4 P2] T-RENDER-4: Image Texture Cache (LRU)**; Goal: reuse SDL textures for repeated images; Scope: SDLGraphicsContext; Acceptance: repeated image paints reuse textures; Tests: renderer perf tests.
