## Rendering Performance Roadmap

- **Milestone 3 (Navigator):** Tracked in `doc/milestones/milestone3.md` (Epic 3.9).
- **Milestone 4 (Scripting):** Introduce retained display list to avoid rebuilding paint commands for static content.
- **Milestone 5 (Extensions/UI):** Split UI chrome (URL bar) from page rendering so editing the URL bar doesn't repaint the page.
- **Milestone 6 (Speedster):** Add offscreen raster cache + layer invalidation to repaint only dirty regions.
- **Text Rendering Cache:** Cache `FontSetup` (and optionally text textures) in `SDLGraphicsContext` so per-frame paint avoids reloading fonts and rebuilding text textures.

## Typography Follow-Ups

- **Font Face Mapping:** Expand `ComputedStyle::font_face` beyond the current Roboto-only mapping (proper fallback chain + real monospace fonts).

## HTML Tag Coverage

- **Semantic Blocks:** `main`, `section`, `article`, `noscript`
- **Controls:** `button`
- **SVG Core:** `svg`, `g`, `defs`, `path`, `rect`, `circle`, `clippath`

## Legacy HTML Attributes (Stories)

* **Story T-HTML-1: Body Color Attributes**
* **As a** reader of classic HTML pages,
* **I want** `<body bgcolor/text/link/vlink>` to map to background and text/link colors.
* **Acceptance:** Body background + base text/link colors reflect legacy attributes when present.

## CSS Coverage Backlog

- **P2: Selector Coverage:** Add universal selector `*`, descendant combinators, and compound selectors (tag+class/id).
- **P2: Typography + Box Model Extras:** Implement `font-family`, `box-sizing`, and `outline` support (no images/gradients yet).

## CSS/Layout Follow-Ups (Stories)

* **Story T-CSS-1: Length Units (em)**
* **As a** user,
* **I want** CSS `em` lengths to resolve against font size so rules like `width: 10em` apply.
* **Acceptance:** `em` values resolve to pixels for width/height/margins/padding where supported.

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

## Engine / App Split Follow-Ups

- Tracked in `doc/milestones/milestone3.md` (Epic 3.1, Epic 3.6, Epic 3.10).
