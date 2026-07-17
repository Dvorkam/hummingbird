# Changelog

All notable changes to this project will be documented in this file.

## [0.6.0] - 2026-07-17

Milestone 6 — "The Layouter": real-page layout compatibility. The DuckDuckGo HTML
homepage now renders close to a reference browser.

### Added

- **CSS Flexbox** layout: `flex-direction` (+reverse), `flex-wrap` (+wrap-reverse), `justify-content`, `align-items` (including baseline), `flex-grow/shrink/basis`, the `flex` shorthand, and `order`.
- **CSS Grid** layout (MVP): `grid-template-columns/rows` with `px/%/fr/repeat()`, `gap`/`row-gap`/`column-gap`, row-major auto-placement, and line/span placement.
- **`@font-face` web fonts**: local and remote TrueType/OpenType, fetched through the resource pipeline (WOFF2 deferred).
- CSS-wide `inherit`, `var()` for non-color properties, `!important`, sibling combinators (`~`/`+`), and additive `calc()` lengths.
- Legacy CSS compatibility slice: `visibility`, `pointer-events`, `text-shadow`, and legacy `clip: rect()`.
- Visited-link state (`:visited` / `vlink`); absolute centering with opposing insets + auto margins.
- F1 debug hit-inspect (logs the element and its geometry to the console); DOM-budget error page.

### Improved

- Control chrome: border/radius longhands, per-side border colors, and vendor-prefix alias handling (prefixed-property noise is now silenced instead of warned).
- `background` shorthand `position/size` syntax and percentage/`auto` `background-size`.
- Selector matching accelerated with a key-selector index; resource updates coalesced into fewer restyle/layout passes; media conditions re-evaluated on viewport resize.
- Architecture: graphics value types moved out of the platform-port header so the style layer no longer depends on the port; clang-uml architecture diagrams scoped and de-noised.

### Security

- Asset-origin firewall: page-controlled URLs can never reach filesystem/asset APIs (UNC/drive/traversal/absolute forms rejected).
- Raw-text tokenizing for `<script>`/`<style>` so markup-looking strings in JS/CSS no longer spawn phantom tags or resource requests.

### Guardrails

- Real DuckDuckGo HTML snapshot end-to-end regression harness in CI (focus/type/submit/navigate).
- Package dependency-cycle detection in CI.

### Deferred

- WOFF2 web-font decoding, grid `repeat()`-anywhere / `minmax()`, and the perf/memory architecture (offscreen raster cache, tab resource eviction, DOM virtualization) are tracked in `doc/TODOs.md` (M9/M12/M14).

## [0.5.0] - 2026-02-10

### Added

- Multi-tab browser workflow with create/switch/close controls and per-tab isolation.
- Extension host MVP with manifest loading, background script runtime, tab lifecycle events, and CSS injection API.
- Built-in dark-mode extension behavior that applies across ordinary loaded pages.
- Form interaction and submission improvements including autofocus behavior, robust focus/edit handling, and GET/POST submission paths.
- Table layout/readability improvements with additional regression coverage and external-page verification checklist.
- `BrowserApp` decomposition via `BrowserChrome` and `TabController` to reduce app-level orchestration bloat.

### Improved

- CSS/layout compatibility slice for real pages (including typography and control polish areas used by DDG-like pages).
- Font-family fallback handling for collapsed/quoted family lists to reduce false fallback warnings.
- Documentation for Milestone 5 outcomes and carryover stories.

### Deferred

- Remaining DDG visual parity gaps and snapshot-based DDG end-to-end regression harness are tracked in Milestone 6 backlog (`doc/TODOs.md`).
