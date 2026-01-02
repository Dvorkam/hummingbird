## Rendering Performance Roadmap

- **Milestone 3 (Navigator):** Add basic paint instrumentation and viewport culling (done for paint, extend to debug overlays).
- **Milestone 4 (Scripting):** Introduce retained display list to avoid rebuilding paint commands for static content.
- **Milestone 5 (Extensions/UI):** Split UI chrome (URL bar) from page rendering so editing the URL bar doesn't repaint the page.
- **Milestone 6 (Speedster):** Add offscreen raster cache + layer invalidation to repaint only dirty regions.

## Typography Follow-Ups

- **Font Face Mapping:** `TextBox::resolve_text_font_path()` ignores `ComputedStyle::font_face` and always picks Roboto variants; add a mapping table for known faces (fallback to Roboto with warning).

## Table/Layout Follow-Ups

- **Table Cell Block Alignment:** `text-align` only offsets inline runs; add centering/right alignment for block-level children inside table cells (ACME header mismatch).
