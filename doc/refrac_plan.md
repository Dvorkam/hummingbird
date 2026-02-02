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
