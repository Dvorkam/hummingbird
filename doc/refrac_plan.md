# Refactor Plan

## Goals
1. Evaluate module folder size and future growth vs. roadmap; propose grouping when needed.
2. Identify reusable logic across related modules to reduce redundancy.
3. Review test coverage and constitution adherence across modules.
4. Reassess CSS property registration flow (string/enum/behavior split) and propose improvements.

## Scope
- Folders: app, core, engine, html, layout, platform, renderer, style, tests, doc.
- Focus on high-churn areas first (style/layout/engine), then supporting modules.

## Process (Phased)

### Phase 1: Module Size & Structure Audit
- Inventory each top-level folder:
  - File count, rough responsibilities, roadmap growth expectations.
  - Identify folders with broad/overloaded responsibilities.
- Output:
  - A short table per folder: count, growth risk (low/med/high), proposed grouping (if any).
  - Proposed subfolder structure where warranted (e.g., layout/inline, layout/table already exist).

### Phase 2: Redundancy & Reuse Scan (Primary Focus)
- For each folder group, list repeated patterns and candidate utilities.
- Prioritize:
  - Layout sizing & metrics
  - Inline/baseline logic
  - Control rendering/input handling
  - Style parsing/normalization
  - Resource fetch/resolve patterns
- Output:
  - “Candidate refactors” list with:
    - Files involved
    - Reuse opportunity summary
    - Suggested utility/module location
    - Risk level (low/med/high)
    - Quick acceptance criteria

### Phase 3: Test Coverage & Constitution Compliance
- For each module group:
  - Map features to tests (gaps flagged).
  - Check alignment with constitution rules (module boundaries, string hygiene, magic numbers, etc.).
- Output:
  - Coverage gaps list with suggested tests.
  - Constitution deviation list with fix ideas.

#### Phase 3 Plan (High Level)
- Scope pass 1: verify high‑level test coverage for each module group (app, core, engine, html, layout, platform, renderer, style).
- Scope pass 2: check constitution adherence per group (module boundaries, error handling, logging, naming, string handling, magic numbers).
- Scope pass 3: cross‑cutting checks (shared utilities usage, duplication hot‑spots, missing integration tests).
- Output: a short checklist per group with green/yellow/red status, and a list of candidate fixes (to be detailed in Phase 3 step 2).

#### Phase 3 Candidates (Scope Pass 1)

### P3-01 [ ] app: UrlBar editing coverage
- Files: `src/app/UrlBar.cpp`, tests missing.
  - Gap: no unit coverage for caret movement, UTF‑8 insert/delete, clipboard paste path, or submit/cancel behavior.
  - Suggested tests: key handling (left/right/home/end/backspace/delete/enter/escape), UTF‑8 insertion, paste path with mock window.

### P3-02 [ ] core/utils: TextEditBuffer tests
- Files: `src/core/utils/TextEditBuffer.*`, tests missing.
  - Gap: no direct unit coverage for UTF‑8 caret clamp/insert/delete paths.
  - Suggested tests: ASCII + multi‑byte (e.g., “á”, emoji) insert/delete/backspace/forward-delete semantics.

### P3-03 [ ] layout: RenderObject sanity tests
- Files: `src/layout/RenderObject.*`, `tests/layout/RenderObject.test.cpp` (currently commented out).
  - Gap: RenderObject tests disabled; no coverage for inline/block status helpers.
  - Suggested tests: re‑enable and validate inline status reporting.

### P3-04 [ ] platform/graphics: cache eviction behavior
- Files: `src/platform/graphics/SDLGraphicsContext.cpp`, `src/platform/graphics/Blend2DFontCache.cpp`
  - Gap: no tests for cache eviction logic (LRU ordering, byte caps, texture cleanup).
  - Suggested tests: small cache sizes, verify eviction order + byte accounting; could be unit tests around cache containers if SDL textures are hard to instantiate.

### P3-05 [ ] core/utils: AssetLoader coverage
- Files: `src/core/utils/AssetLoader.*`
  - Gap: no direct tests for AssetLoader path handling, missing file logging gating, URL rejection.
  - Suggested tests: empty id, URL id, missing file (no crash), successful load (fixtures).

### P3-06 [ ] engine/document: DocumentInputController focus/edit coverage
- Files: `src/engine/document/DocumentInputController.cpp`
  - Gap: behavior mostly covered indirectly via EngineTab tests; no focused unit tests for caret clamping or backspace/delete when empty.
  - Suggested tests: focus/clear focus, caret clamp with UTF‑8, delete/backspace on boundaries.

### P3-07 [ ] engine/document: DocumentPainter display list reuse
- Files: `src/engine/document/DocumentPainter.cpp`
  - Gap: no direct tests for display list invalidation/reuse (owner changes, viewport change, scroll/debug flags).
  - Suggested tests: reuse on identical inputs; invalidate on viewport/scroll/debug flag changes.

### P3-08 [ ] engine/resources: ResourceLoader request behavior
- Files: `src/engine/resources/ResourceLoader.cpp`
  - Gap: ResourceLoader behavior covered indirectly via Tab tests; no direct unit coverage for asset fallback/log paths or request option differences.
  - Suggested tests: stylesheet vs image request option wiring, missing asset behavior, fallback network path for documents.

#### Phase 3 Candidates (Scope Pass 2)

### P3C-01 [ ] engine/document: split paint_controls (SLAP + length)
- Files: `src/engine/document/DocumentInputController.cpp`
  - Concern: `paint_controls` is long and mixes traversal, layout math, text measurement, and painting.
  - Suggested refactor: extract caret computation + text paint into helpers; keep traversal high‑level.

### P3C-02 [ ] app: UrlBar key handling helper
- Files: `src/app/UrlBar.cpp`
  - Concern: `handle_key_down` is long and mixes clipboard, editing, and submission logic.
  - Suggested refactor: split into helpers (clipboard handling, editing keys, submit/cancel).

### P3C-03 [ ] app/engine: magic pixel constants audit
- Files: `src/app/UrlBar.cpp`, `src/engine/document/DocumentInputController.cpp`
  - Concern: a few literal pixel values remain (e.g., text baseline offset, caret width/height math).
  - Suggested refactor: replace with named `k...` constants to align with constitution.

#### Phase 3 Candidates (Scope Pass 3)

### P3X-01 [ ] engine/html: form default method + button type gaps
- Files: `tests/engine/Tab.test.cpp`, form submission path (likely `src/engine/document/DocumentModel.cpp`).
  - Gap: TODOs note missing defaults for `method=""` and empty `button type=""`.
  - Suggested fix/tests: implement defaults per HTML spec (GET + submit), add focused tests.

### P3X-02 [ ] layout/platform: monospace font selection TODO
- Files: `src/layout/flow/TextBox.cpp`, `src/layout/flow/TextStyleUtils.h`
  - Gap: monospace font selection is hardcoded; TODO in TextBox.
  - Suggested fix/tests: real font-family list handling for `monospace` with tests in TextStyleUtils or layout.

### P3X-03 [ ] engine/renderer: background-image integration
- Files: `src/engine/document/DocumentPipeline.cpp`, `src/engine/resources/ResourceLoader.cpp`, `src/renderer/Painter.cpp`
  - Gap: background-image is parsed and collected, but no end-to-end test verifying resource fetch + paint.
  - Suggested tests: headless tab or pipeline integration that loads a background image and asserts an image draw call.

### P3X-04 [ ] engine/platform: SVG image pipeline integration
- Files: `src/platform/decoders/SvgImageDecoder.cpp`, `src/engine/resources/ResourceLoader.cpp`, `src/layout/replaced/RenderImage.cpp`
  - Gap: decoder is tested in isolation; no integration coverage for `<img src="...svg">`.
  - Suggested tests: resource load of SVG via Tab or pipeline with render check for decoded dimensions.

### Phase 4: CSS Property Registry Rework (Opinion + Options)
- Current flow: name -> enum -> behavior (multiple files).
- Option A (incremental): keep enums, add a single “property registry” table that includes string, enum, parser, applier.
- Option B (bigger): remove string/enum split and replace with a single registry (string -> handler + metadata), generate enum or use ID at build time.
- My recommendation:
  - **Option A** for now (low risk): central registry table driving parsing + application while preserving existing enums/strings. This reduces duplication and provides one authoritative place for property wiring without a larger rewrite.
- Output:
  - Concrete proposal with tradeoffs and migration steps.

## Deliverables
- This plan (doc/refrac_plan.md).
- A follow-up “scaffolding” doc listing refactor candidates with owners/priority (to be created after Phase 1–3 review).

## Acceptance Criteria
- Clear, agreed structure changes (if needed).
- Identified and prioritized redundancy refactors.
- Test coverage gaps documented.
- Decision on CSS property registration approach.

## Phase 1 Findings (Module Size & Structure Audit)

### Summary (File Counts)
- app: 6
- core: 35 (subfolders: dom/, utils/, platform_api/)
- engine: 21 (subfolder: script/)
- html: 11
- layout: 44 (subfolder: inline/)
- platform: 32 (subfolder: script/)
- renderer: 4
- style: 18

### Candidates for Structure Grouping

#### core (35, medium growth)
Already has dom/, utils/, platform_api/. No immediate restructure needed. Growth likely in utils + dom; consider subfolders in utils if file count grows > ~20 (e.g., utils/strings, utils/net).

#### engine (21, medium growth, roadmap suggests growth)
Likely growth: input handling, pipeline, resource orchestration, scripting integration. Candidate grouping:
- engine/document/ (DocumentModel, DocumentPipeline, DocumentPainter, DocumentResources, DocumentInputController)
- engine/resources/ (ResourceLoader, ResourceStore)
- engine/script/ (already present)
- engine/tab/ (Tab)

#### layout (44, high growth)
Layout is already large and will grow with flex/grid. Candidate grouping:
- layout/inline/ (already)
- layout/block/
- layout/table/
- layout/controls/ (RenderRule, RenderBreak, control-related renderers)
- layout/replaced/ (RenderImage, RenderSvg, future video/canvas)
- layout/metrics/ (LayoutMetricsUtils, ReplacedElementUtils, InlineBaselineUtils)

#### platform (32, medium growth)
Candidates:
- platform/net/ (CurlNetwork, NetworkThreadPool, NetworkFactory)
- platform/graphics/ (SDLGraphicsContext, Blend2DFontCache, SDLWindow)
- platform/decoders/ (SDLImageDecoder, SvgImageDecoder, CompositeImageDecoder)
- platform/script/ (already present)

#### style (18, medium growth)
Candidates:
- style/parser/ (CssParser, CssTokenizer)
- style/selector/ (SelectorMatcher)
- style/registry/ (CssPropertyRegistry, CssPropertyNames, CssValueNames)
- style/compute/ (StyleEngine, StyleDefaults, ComputedStyle)

#### html (11, low growth)
No grouping needed yet. If tokenizer/parser grow, consider html/parser/ and html/tokens/.

#### renderer (4, low growth)
No grouping needed.

#### app (6, low growth)
No grouping needed.

### Phase 1 Implementation (Module Grouping Applied)
Implemented the proposed groupings:
- engine/document, engine/resources, engine/tab
- layout/block, layout/controls, layout/replaced, layout/table, layout/formatting, layout/flow, layout/geometry, layout/paint (with layout/geometry/metrics)
- platform/net, platform/graphics, platform/decoders, platform/resources
- style/parser, style/selector, style/registry, style/compute


## Phase 2 Pass Order
src/layout/flow (incl. src/layout/flow/inline)
src/layout/geometry (incl. src/layout/geometry/metrics)
src/layout/formatting
src/layout/replaced
src/layout/block
src/layout/table
src/layout/controls
src/layout/paint
src/style/parser
src/style/compute
src/style/registry
src/style/selector
src/engine/document
src/engine/resources
src/engine/tab
src/platform/net
src/platform/decoders
src/platform/graphics
src/platform/resources
src/html
src/renderer
src/core (subfolders: dom, utils, platform_api)
src/app

## Phase 2 Candidates

### R2-01 [x] layout: text metrics helper
- Files: `src/layout/flow/TextBox.cpp`, `src/layout/geometry/metrics/InlineBaselineUtils.h`, `src/layout/controls/RenderBreak.cpp`
  - Reuse: text ascent + line-height resolution logic duplicated (TextBox::compute_text_ascent vs InlineBaselineUtils::estimate_text_ascent and RenderBreak’s line-height fallback).
  - Suggested utility: `layout/geometry/metrics/TextMetricsUtils.h` (e.g., `resolve_line_height(style, metrics)` + `resolve_ascent(metrics, line_height)`).
  - Risk: low.
  - Acceptance: baseline-alignment + underline tests continue to pass.

### R2-02 [x] layout: text layout helpers
- Files: `src/layout/flow/TextBox.cpp`
  - Reuse: whitespace collapse + tokenization + rendered-text normalization is local-only but likely reusable by future text nodes.
  - Suggested utility: `layout/flow/TextLayoutUtils.h` (move `collapse_whitespace`, `tokenize_text`, `build_rendered_text`).
  - Risk: low.
  - Acceptance: TextBox behavior unchanged; tests unchanged.

### R2-03 [x] layout: shared available-width computation
- Files: `src/layout/flow/TextBox.cpp`, `src/layout/table/RenderTable.cpp`, `src/layout/geometry/metrics/LayoutMetricsUtils.h`
  - Reuse: available-width calculation duplicates `Metrics::content_width` + CSS width overrides.
  - Suggested utility: extend `LayoutMetricsUtils` with `compute_available_width(style, bounds, insets)` or `resolve_content_width(...)`.
  - Risk: low.
  - Acceptance: TextBox + Table layout unchanged.

### R2-04 [x] layout: shared block/float/inline flow helpers
- Files: `src/layout/block/BlockBox.cpp`, `src/layout/formatting/RenderListItem.cpp`
  - Reuse: nearly identical block/inline/float flow logic (child margins, float placement, inline group layout, line cursor flushing, float-band handling).
  - Suggested utility: `layout/formatting/FlowLayoutUtils.*` (or `layout/block/BlockFlowUtils.*`) with shared helpers:
    - `compute_child_margins(...)`
    - `resolve_float_type(...)`
    - `layout_block_child(...)`
    - `layout_float_child(...)`
    - `layout_inline_group(...)`
    - `resolve_line_height_hint(...)`
  - Risk: medium.
  - Acceptance: existing list/float layout tests pass; visual layout unchanged in StubNetwork.

### R2-05 [x] layout: replaced sizing helper
- Files: `src/layout/replaced/RenderImage.cpp`, `src/layout/replaced/RenderSvg.cpp`
  - Reuse: duplicated `SizeOptions` assembly + `compute_layout_size` calls in `layout()` and `measure_inline()`.
  - Suggested utility: `ReplacedElementUtils::resolve_layout_size(...)` or a small helper that accepts default size + intrinsic dims and returns `LayoutSize`.
  - Risk: low.
  - Acceptance: image/svg size tests pass; inline sizing unchanged.

### R2-06 [x] layout: traversal helper consolidation
- Files: `src/layout/paint/RenderTreeTraversal.h`, `src/layout/geometry/PositioningUtils.h`
  - Reuse: tree traversal + z-order traversal coupling suggests consolidating traversal helpers into a single utility namespace to avoid drift.
  - Suggested utility: `layout/paint/TraversalUtils.*` (merge shared traversal patterns, keep z-order in Positioning).
  - Risk: low/medium.
  - Acceptance: paint order tests (if any) unchanged.

### R2-07 [x] style: shared value parsing helpers
- Files: `src/style/parser/CssParser.cpp`, `src/style/compute/StyleEngine.cpp`, `src/style/registry/CssValueNames.h`
  - Reuse: length parsing + token splitting (`parse_length_token`, `parse_length_or_number`, `split_tokens`, `value_to_length`) and value parsing helpers are duplicated.
  - Suggested utility: `style/compute/StyleValueUtils.*` (shared `split_tokens`, `parse_length_*`, `value_to_length`) and/or `style/parser/CssValueParser.*` (color/identifier helpers).
  - Risk: low/medium.
  - Acceptance: CSSParser + StyleEngine tests pass unchanged.

### R2-08 [x] style: registry-driven property application (Phase 4 Option A)
- Files: `src/style/registry/CssPropertyRegistry.cpp`, `src/style/compute/StyleEngine.cpp`, `src/style/parser/CssParser.cpp`
  - Reuse: property name → enum → handling split across files; registry not used to drive application.
  - Suggested utility: extend `CssPropertyRegistry` to include metadata + handler pointers; replace long apply chain with a property-applier table.
  - Risk: medium.
  - Acceptance: all style tests pass; computed style output unchanged.

### R2-09 [x] style/selector: shared whitespace token splitting
- Files: `src/style/selector/SelectorMatcher.cpp`, `src/core/utils/StringUtils.h`
  - Reuse: class splitting / whitespace tokenization is custom; similar logic exists in other modules.
  - Suggested utility: `Core::Utils::split_ascii_whitespace` used by selector + style.
  - Risk: low.
  - Acceptance: selector matching tests pass unchanged.

### R2-10 [x] engine: shared hit-test traversal
- Files: `src/engine/document/DocumentPipeline.cpp`, `src/engine/document/DocumentInputController.cpp`
  - Reuse: hit-test traversal pattern duplicated (z-order traversal with early-exit + viewport culling).
  - Suggested utility: `engine/document/HitTestUtils.*` (wrap common traversal + culling; accept predicate callbacks).
  - Risk: low/medium.
  - Acceptance: link/input/form hit tests unchanged.

### R2-11 [x] engine: pipeline step helper
- Files: `src/engine/document/DocumentPipeline.cpp`, `src/engine/tab/Tab.cpp`
  - Reuse: repeated “apply styles + layout + update layout state + dirty” flows.
  - Suggested utility: `DocumentPipeline::rebuild_and_layout(...)` or a small `PipelineSteps` helper.
  - Risk: medium.
  - Acceptance: no change in pipeline timing logs or invalidation behavior.

### R2-12 [x] engine/resources: request options helper
- Files: `src/engine/resources/ResourceLoader.cpp`, `src/engine/resources/ResourceStore.cpp`
  - Reuse: request state transitions and logging are split; `ResourceLoader::request_resources` has repeated option setup for styles/images.
  - Suggested utility: `ResourceRequestBuilder` (prebaked options for Stylesheet/Image) or small helper constructors.
  - Risk: low.
  - Acceptance: resource request logging + state transitions unchanged.

### R2-13 [x] engine/tab: layout state helper
- Files: `src/engine/tab/Tab.cpp`, `src/engine/document/DocumentPipeline.cpp`
  - Reuse: viewport change handling + relayout + state update could be centralized.
  - Suggested utility: `TabLayoutState` (track viewport, content height, scroll clamping).
  - Risk: low/medium.
  - Acceptance: scroll clamp behavior unchanged.

### R2-14 [x] core: shared asset loader
- Files: `src/platform/net/StubNetwork.cpp`, `src/platform/resources/FileResourceProvider.cpp`, `src/core/utils/AssetPath.*`
  - Reuse: asset file loading logic duplicated (resolve asset path + read bytes).
  - Suggested utility: `core/utils/AssetLoader.*` (`load_asset_bytes` / `load_asset_text`) used by both stub network and file provider.
  - Risk: low.
  - Acceptance: stub network + file provider tests unchanged.

### R2-15 [x] platform/decoders: image decode helpers
- Files: `src/platform/decoders/SDLImageDecoder.cpp`, `src/platform/decoders/SvgImageDecoder.cpp`
  - Reuse: constructing `ImageBitmap` and validating dimensions/stride repeats across decoders.
  - Suggested utility: `platform/decoders/ImageDecodeUtils.*` (helpers to validate dimensions and allocate/copy pixel buffers).
  - Risk: low.
  - Acceptance: image decoder tests unchanged.

### R2-16 [x] platform/graphics: cache eviction helper
- Files: `src/platform/graphics/SDLGraphicsContext.cpp`, `src/platform/graphics/Blend2DFontCache.cpp`
  - Reuse: LRU-style cache eviction logic duplicated (scan entries by last_used).
  - Suggested utility: `core/utils/LruCache.h` or `platform/graphics/CacheUtils.*`.
  - Risk: low/medium.
  - Acceptance: text/image cache behavior unchanged.

### R2-17 [x] platform/script: shared base helpers
- Files: `src/platform/script/QuickJSScriptEngine.cpp`, `src/platform/script/NullScriptEngine.cpp`
  - Reuse: similar interface scaffolding and error path handling.
  - Suggested utility: `ScriptEngineBase` with shared helpers (e.g., build error result).
  - Risk: low.
  - Acceptance: script engine tests unchanged.

### R2-18 [x] html: shared character-class helpers
- Files: `src/html/HtmlTokenizer.cpp`, `src/html/HtmlParser.cpp`, `src/html/HtmlStringUtils.h`
  - Reuse: repeated character-class logic (whitespace checks, tag/attribute name character tests).
  - Suggested utility: expand `HtmlStringUtils` (e.g., `is_name_char`, `skip_whitespace`) used by tokenizer + parser.
  - Risk: low.
  - Acceptance: HtmlTokenizer/HtmlParser tests unchanged.

### R2-19 [x] renderer: command helpers
- Files: `src/renderer/Painter.cpp`, `src/renderer/DisplayList.cpp`
  - Reuse: draw command wiring (fill rect, draw image/text) could be centralized into a small helper so Painter and DisplayList stay consistent.
  - Risk: low.
  - Acceptance: renderer tests unchanged.

### R2-20 [x] app/core: shared UTF-8 text editing helper
- Files: `src/app/UrlBar.cpp`, `src/engine/document/DocumentInputController.cpp`
  - Reuse: UTF-8 text editing (insert/delete/caret movement) duplicated at a high level.
  - Suggested utility: `core/utils/TextEditBuffer.*` wrapping caret + UTF-8 mutations for reuse in URL bar and form inputs.
  - Risk: low/medium.
  - Acceptance: URL bar + input editing behavior unchanged.
