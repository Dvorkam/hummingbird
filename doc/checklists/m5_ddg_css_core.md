# DDG HTML Homepage — Manual CSS/Layout Checklist

Manual regression checklist for the DuckDuckGo HTML homepage
(`https://html.duckduckgo.com/html/`), the milestone's North Star page. Named
`m5_ddg_css_core.md` for continuity with the M5 TODO that first referenced it;
the work landed in Milestone 6 (`T-DDG-CSS-CORE-2`).

Most of this is now also covered automatically:
- **Layout invariants** — `tests/layout/DdgHomeLayout.test.cpp` (centering, on-screen, input/button sizing).
- **Interaction flow** — `tests/engine/Tab.test.cpp :: DdgHomepageSnapshotFocusTypeSubmitFlow` (autofocus → type → submit).

Run this manual pass when touching CSS parsing, box/flex layout, form hit-testing,
or the paint path, and before tagging a release. The pinned snapshot lives in
`tests/fixtures/ddg/`.

## How to run

1. Build: `scripts\build.ps1` (Windows) / the Linux preset.
2. Launch the app and navigate to `https://html.duckduckgo.com/html/`.
3. Use a desktop-sized window (~1024×768). For a clean read, first check with the
   dark-mode extension **disabled** (`HB_EXTENSIONS_DISABLE=dark-mode`), then again
   enabled once `!important` (T-CSS-IMPORTANT-1) lands.

## Layout — centered logo + search block

- [ ] The DuckDuckGo duck **logo renders as a proper circle** (with the "DuckDuckGo" wordmark from the SVG), **not a squished vertical ellipse** — regression guard for percentage `background-size`.
- [ ] Logo and search form are **horizontally centered** and sit in the upper-middle band (~top 24%).
- [ ] Nothing hangs off the **left edge** (the pre-flex failure mode).
- [ ] The **search input** is a clickable bar (height ~30–80px, width > 300px), not a collapsed sliver.
- [ ] The **submit button** is a small control next to the input, not an oversized box blanketing the page.
- [ ] The input's rounded corners / border look like a real control (no square seams).

## Interaction — focus / type / submit

- [ ] On load the **search box is autofocused** (caret visible without clicking).
- [ ] Typing shows the text **as you type**, with a caret; deleting removes it immediately.
- [ ] **Enter** submits: navigates to `.../html/` with the query (`POST`, body contains `q=<query>`).
- [ ] Clicking the **magnifier button** submits the same way.
- [ ] Clicking **empty page area** does nothing (does not submit / reload).

## Known-deferred (expected NOT-yet; do not file as new bugs)

Each already has a story; note the ticket if the symptom regresses further.

- [ ] Magnifier sits slightly outside the form box — **T-POS-ABS-1** (absolute centering w/ opposing insets).
- [ ] Magnifier background does not turn green on input focus — **T-CSS-SIBLING-1** (`~` combinator).
- [ ] Magnifier icon glyph missing (icon font) — **T-FONT-FACE-1** (`@font-face`).
- [ ] Corner radius / max-width driven by `var()` fall back to defaults — **T-CSS-VAR-3**.
- [ ] Dark-mode extension does not fully theme the page — **T-CSS-IMPORTANT-1** (`!important`).
- [ ] `font-family: inherit` logs a warning and falls back to Roboto — **T-CSS-INHERIT-1**.
- [ ] `calc()` sizes fall back / are ignored — **T-CSS-CALC-1**.
- [ ] Hover/focus transitions do not animate — **T-ANIM-1** (static-only, P2).

## Sign-off

- Date / build:
- Extension state (disabled / enabled):
- Result (pass / deltas):
- Notes:
