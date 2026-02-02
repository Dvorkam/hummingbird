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

