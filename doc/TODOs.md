## Rendering Performance Roadmap

- **Milestone 3 (Navigator):** Tracked in `doc/milestones/milestone3.md` (Epic 3.9).
- **Milestone 4 (Scripting):** Introduce retained display list to avoid rebuilding paint commands for static content.
- **Milestone 5 (Extensions/UI):** Split UI chrome (URL bar) from page rendering so editing the URL bar doesn't repaint the page.
- **Milestone 6 (Speedster):** Add offscreen raster cache + layer invalidation to repaint only dirty regions.
- **Text Rendering Cache:** Cache `FontSetup` (and optionally text textures) in `SDLGraphicsContext` so per-frame paint avoids reloading fonts and rebuilding text textures.

## Typography Follow-Ups

- **Font Face Mapping:** Expand `ComputedStyle::font_face` beyond the current Roboto-only mapping (proper fallback chain + real monospace fonts).

## CSS Coverage Backlog

- **P0: Shorthand Aliases for `background` + `border`:** Parse `background` (color-only) as `background-color` and `border` as width/style/color when all tokens are present.
- **P1: Margin/Padding Multi-Value + Auto:** Support 2/3/4-value shorthands and `auto` for horizontal centering (`margin: 8px auto`).
- **P1: Apply `font-size` + `line-height`:** Thread parsed values into computed style and layout measurements to match real text flow.
- **P1: `max-width` Enforcement:** Apply `max-width` during layout to cap block widths.
- **P2: Selector Coverage:** Add universal selector `*`, descendant combinators, and compound selectors (tag+class/id).
- **P2: Typography + Box Model Extras:** Implement `font-family`, `box-sizing`, and `outline` support (no images/gradients yet).

## Table/Layout Follow-Ups

- **Table Cell Block Alignment:** `text-align` only offsets inline runs; add centering/right alignment for block-level children inside table cells (ACME header mismatch).

## Engine / App Split Follow-Ups

- Tracked in `doc/milestones/milestone3.md` (Epic 3.1, Epic 3.6, Epic 3.10).
