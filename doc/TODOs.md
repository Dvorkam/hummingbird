## Rendering Performance Roadmap

- **Milestone 3 (Navigator):** Add basic paint instrumentation and viewport culling (done for paint, extend to debug overlays).
- **Milestone 4 (Scripting):** Introduce retained display list to avoid rebuilding paint commands for static content.
- **Milestone 5 (Extensions/UI):** Split UI chrome (URL bar) from page rendering so editing the URL bar doesn't repaint the page.
- **Milestone 6 (Speedster):** Add offscreen raster cache + layer invalidation to repaint only dirty regions.

## 2.5.2 Refactor Coverage Plan

- **Inventory & Baseline:** Generate a top list of largest/mixed-abstraction functions and tag them with owner modules.
- **Core Module Pass:** Refactor `src/core` for single-level abstraction and extract low-level helpers.
- **HTML Module Pass:** Refactor `src/html` parsing and tokenizer flows with clear high/low separation.
- **Style Module Pass:** Refactor `src/style` parsing/cascade into orchestrators + helpers.
- **Layout Module Pass:** Refactor `src/layout` block/inline layout into clean phases (collect, measure, place).
- **Renderer Module Pass:** Refactor `src/renderer` paint flow to isolate traversal vs drawing details.
- **App Module Pass:** Refactor `src/app` orchestration to remove low-level logic from high-level flow.
- **Platform Module Pass:** Refactor `src/platform` to keep API glue thin and push details into helpers.
- **Verification Sweep:** Re-scan for long/mixed functions and confirm no regressions via unit tests.

## Follow-ups

- **StyleEngine UA Defaults:** Move `apply_ua_defaults` to a data-driven table or dedicated module to keep tag growth manageable.
