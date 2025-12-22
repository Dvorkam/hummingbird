# Hummingbird Engine - Project Roadmap

This document outlines the high-level path from an empty repository to a modular, extension-ready browser engine. The philosophy is "Agile Iteration": strictly modular components, minimal memory footprint, and progressive complexity. Current codebase modules live in `src/app`, `src/core`, `src/html`, `src/style`, `src/layout`, `src/renderer`, and `src/platform`; keep Ports & Adapters intact (Core/HTML/Layout never depend on Platform).

---

## Milestone 0: The Skeleton (Foundation)
**Theme:** *Architecture & Abstraction* **Goal:** Establish the build system, core utilities, and platform abstractions without implementing browser logic yet.

* **Build System:** CMake/Meson setup separating `Core`, `Platform`, and `App`.
* **Memory Model:** Implement `ArenaAllocator` for bulk DOM management.
* **Platform Abstraction:**
    * `IWindow`: Abstract interface for OS windows.
    * `IGraphicsContext`: Abstract interface for 2D drawing.
    * **Implementation:** SDL2 (Window) + Blend2D (Graphics).
* **Deliverable:** A "Hello Engine" executable that opens a window and clears the screen.

---

## Milestone 1: The Reader (HTML MVP)
**Theme:** *Data to Pixels (MVC Pattern)* **Goal:** Render static HTML with hardcoded "User Agent" defaults and a working render tree. Current status: Epics 1.1–1.3 shipped; Epic 1.4 (flow/layout refinement) is next.

* **HTML Pipeline (done):**
    * Zero-copy tokenizer (`std::string_view`).
    * DOM builder with open-element stack, void handling, and unsupported-tag logging.
* **Style Defaults (done):**
    * Hardcoded mapping for headings, lists (ul/ol indent), links (blue/underline), code, pre, hr, blockquote, strong/em.
* **Layout Engine (partial):**
    * Block layout: vertical stacking with margin/padding support.
    * Inline layout: single-line flow for inline elements and text; BR/HR control objects.
    * Next: render-tree construction refinements, greedy line breaking, and viewport-aware block flow.
* **Viewport (upcoming within 1.4/1.7):**
    * Greedy text wrapping.
    * Basic scrolling/camera offset.
* **Deliverable:** Visualize `mfw.html` legibly with headings, lists, inline links/code, and rules.

---

## Milestone 2: The Stylist (CSS & Rendering)
**Theme:** *Separation of Concerns* **Goal:** Replace hardcoded C++ styles with an external stylesheet pipeline.

* **CSS Parser (partial):** Tokenize/parse basic selectors and declarations (already present, will expand coverage).
* **Cascade:** Apply selector specificity and compute styles for every node.
* **Box Model Refinement:** Proper margins/padding/borders; `display: none` / `inline-block`.
* **Deliverable:** Pages render differently based on a `.css` file; real bullets for lists via pseudo-element logic.

---

## Milestone 3: The Navigator (The Web)
**Theme:** *Connectivity & Interaction* **Goal:** Connect the engine to the internet and handle page transitions.

* **Networking Layer:**
    * Implement `INetwork` using **libcurl** (Multi-interface).
    * **Concurrency:** Introduce the **IO Thread** to keep networking off the main UI loop.
* **Interactivity:**
    * **Hit Testing:** Determine which `RenderObject` is under the mouse.
    * **Link Logic:** Clicking `<a>` triggers a network fetch and DOM tear-down/re-build.
* **UI:**
    * A simple URL bar and "Go" button.
* **Deliverable:** A functional browser that can navigate from Wikipedia Home to an Article.

---

## Milestone 4: The Brain (Scripting)
**Theme:** *Logic & State* **Goal:** Embed a JavaScript engine to allow dynamic page content.

* **JS Engine:** Integrate **QuickJS** (small footprint, easier C API).
* **Bindings:**
    * Expose `document`, `window`, and `console` to JS.
    * Allow JS to modify the DOM (triggering re-layout).
* **Events:** Hook up `onclick`, `onload` events from C++ to JS.
* **Deliverable:** A page with a button that changes text color when clicked.

---

## Milestone 5: The Architect (Extensions & UI)
**Theme:** *Extensibility* **Goal:** Create the "Browser OS" layer that manages tabs and plugins.

* **Extension API:**
    * Create the `browser.*` JS API namespace.
    * Allow loading "Background Scripts" (extensions) alongside the web page.
* **Tab Management:**
    * Isolate pages into separate `Tab` objects (logical isolation).
* **Deliverable:** A browser that supports a "Dark Mode" extension which injects CSS into every loaded page.

---

## Milestone 6: The Speedster (Optimization)
**Theme:** *Performance & Parallelism* **Goal:** Move from "functional" to "fast."

* **Parallelism:**
    * Move Paint Command generation to a separate thread?
    * **Raster Thread:** Execute GPU commands separately from DOM logic.
* **Compositing:**
    * Implement layers (scrolling doesn't repaint the whole page, just moves a texture).
* **Deliverable:** Smooth 60fps scrolling on complex pages.
