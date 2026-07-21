# Developer Guides

This folder contains implementation workflow guides for recurring engineering tasks.

## Available guides

- `css_property_workflow.md`: how to add CSS properties with the registry-based parser/applier system.
- `architecture_diagrams.md`: how the clang-uml architecture diagrams are scoped, and how to regenerate them.
- `dom_arena_ownership.md`: who owns arena-backed DOM nodes once JS can create/move/remove them, and how wrappers survive navigation (M7).
- `spec_conformance_strategy.md`: why we build demo-driven now vs. conformance-first later, and the disciplines (deviation register, per-module conformance slices, WPT) that make the eventual turn cheap.
- `form_control_workflow.md`: how to add a small native form control without silently pulling later Forms v2 semantics forward.

The per-spec adherence registers those disciplines produce live one level up in
`doc/conformance/` — start at its `README.md`.

## Usage rule

When a task matches one of these workflows, follow the guide before implementing code changes.
If a repeated workflow has no guide yet, add one in this folder.
