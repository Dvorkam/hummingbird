#Hummingbird Engine - Project Roadmap

This document outlines the high - level path from an empty repository to a modular,
    extension - ready browser engine.The philosophy is "Agile Iteration" : strictly modular components,
    minimal memory footprint,
    and progressive complexity.Current codebase modules live in `src / app`, `src / core`, `src / html`, `src / style`, `src / layout`, `src /
                                                                                                                                            renderer`,
    and `src / platform`; keep Ports & Adapters intact (Core/HTML/Layout never depend on Platform).

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
* **Box Model Refinement:** Proper margins/padding/borders;
`display : none` / `inline - block`.*** Deliverable : **Pages render differently based on a `.css` file; real bullets for lists via pseudo-element logic.

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

---

## Milestone 7: The DOMster (DOM + Events Core)

**Theme:** *Stateful Documents*
**Goal:** Turn the HTML tree into a real, mutable DOM with a coherent event system and deterministic invalidation (style/layout/paint).

* **DOM Core:**

  * Node/Element/Text model hardened (attributes, `classList`, basic traversal).
  * DOM mutation primitives (append/remove/replace) with strict arena ownership.
* **Event System v1:**

  * Capture/target/bubble propagation.
  * Pointer + keyboard events routed from Platform → Core via interfaces.
* **Invalidation Model:**

  * Mutation marks style/layout/paint dirty regions (no “rebuild everything” on every change).
* **Deliverable:** A toy SPA (no network) that builds UI via JS + DOM, responds to clicks, and updates layout correctly.

---

## Milestone 8: The Scripter++ (JS Bindings + Timers + Microtasks)

**Theme:** *Programmable Web*
**Goal:** Make JS-driven pages realistic: timers, microtasks, and enough DOM bindings to run small frameworks-lite patterns.

* **JS Runtime Integration:**

  * One runtime per document/tab; strict boundary (no exceptions across C++).
  * GC interaction rules documented (who owns what; arena-backed native nodes).
* **Web API Surface v1:**

  * `window`, `document`, `console`, `Element` basics.
  * `addEventListener/removeEventListener`, event objects.
* **Scheduling:**

  * `setTimeout/setInterval`, task queue.
  * Microtask queue (Promise jobs) integrated with main loop tick.
* **Deliverable:** “TodoMVC-class” app where UI is generated by JS, with stable performance and no DOM corruption.

---

## Milestone 9: The Networker (Phased: Cookies → Fetch → Cache → History)

**Theme:** *App-Grade Connectivity*
**Goal:** Support modern web app networking patterns in incremental, testable steps.

This milestone is intentionally split into phases so it doesn’t become a “big bang” rewrite.

### Milestone 9A: Cookies v1 + Redirect/Response Plumbing

* **Cookies (storage + policy)**
  * Domain/path matching, Secure/HttpOnly.
  * SameSite baseline (Lax/Strict minimum).
* **Redirect behavior**
  * Follow redirects with correct cookie semantics.
* **Deliverable:** A site that relies on cookies across navigations works (even if no JS fetch yet).

### Milestone 9B: Fetch/XHR v1 + CORS (Strict)

* **Fetch/XHR v1**
  * Request/response headers, redirects, buffering (streaming later).
* **CORS v1**
  * Start strict; expand behind feature flags.
* **Deliverable:** A JS app can make API calls (GET/POST) and render based on responses.

### Milestone 9C: HTTP Cache v1 (Memory First)

* **Cache basics**
  * ETag / If-None-Match, Cache-Control baseline, in-memory cache first.
* **Deliverable:** Reloading an app reduces network traffic and time-to-interactive.

### Milestone 9D: Navigation Model v2 (History + Lifecycle)

* **History baseline**
  * back/forward, reload, and stable lifecycle without leaks or dangling `std::string_view`.
  * push/replace state can be partial initially.
* **Deliverable:** A JS-heavy site survives refresh/back/forward without corrupting document state.

---

## Milestone 10: The Layouter (Flexbox + Fixed/Sticky + Scroll Containers)

**Theme:** *Modern Layout Primitives*
**Goal:** Implement the layout features that modern UIs assume by default.

* **Flexbox v1:**

  * Row/column, main/cross alignment, basic flex factors.
* **Positioning v1:**

  * `position: fixed` (non-negotiable), `absolute` correctness improvements.
  * `sticky` (phase 2 if needed, but planned in architecture now).
* **Overflow + Scrolling v2:**

  * Real scroll containers (not just document scroll), clip/scroll offsets.
  * Hit-testing respects scroll transforms.
* **Stacking Context v1:**

  * z-index basics to keep headers/menus sane.
* **Deliverable:** Gmail-like header/sidebar layout renders correctly and scrolls without repainting the world.

---

## Milestone 11: The Inputter (Forms v2, Focus, Selection, Clipboard)

**Theme:** *Human Interaction*
**Goal:** Make login and composing text practical (this is where “viable” often lives or dies).

* **Form Controls v2:**
  * Milestone 4 covers “forms MVP” (basic input/button + GET submit). This milestone expands it into “usable for real logins”.
  * `<input type=text/password>`, `<textarea>`, `<button>` behavior.
  * Value editing, selection, caret movement, copy/paste.
* **Focus System:**

  * Tab order, focus rings (style optional), keyboard routing.
* **Composition Plan:**

  * Latin input “good enough” first.
  * IME/composition events as a follow-up epic (kept behind interfaces).
* **Deliverable:** Log into a major site reliably and type into rich-ish forms without constant glitches.

---

## Milestone 12: The Media Engine (Images + Video Playback, MVP First)

**Theme:** *Pixels That Move*
**Goal:** Make media functional first, then rely on compositor work for “smooth”.

* **Image Pipeline v2:**

  * Decoding breadth (jpeg/png/gif/webp as planned), caching, and correct sizing.
* **`<video>` v1:**

  * Decode + present frames, audio output, basic controls.
  * Fullscreen (platform adapter).
* **Streaming Strategy:**

  * Phase 1: progressive MP4/HLS-if-easy.
  * Phase 2 (likely required for real YouTube): MSE/DASH support.
* **Deliverable:** Videos play on a “watch page” (performance may be limited); “smooth feed scrolling” is a Milestone 13/14 goal.

---

## Milestone 13: The Compositor (Retained Rendering + Layers)

**Theme:** *Don’t Repaint the World*
**Goal:** Make scrolling and animations usable on JS-heavy pages.

* **Retained Display List:**

  * Build once, diff on invalidation; stable IDs for render nodes.
* **Layer Tree:**

  * Scroll layers move as textures; minimal repaint.
* **Raster Strategy:**

  * Keep DOM/layout on main thread; optionally add a raster thread behind Platform later.
* **Deliverable:** 60fps-class scrolling on heavy feeds and mail lists on mid-tier hardware (enables the “smooth media” targets in Milestone 14).

---

## Milestone 14: The “Viable” Demo (Gmail + Instagram + YouTube Targets)

**Theme:** *Proof Over Promises*
**Goal:** Turn “it should work” into a measurable compatibility claim with explicit limitations.

* **Target Matrix + Feature Flags:**

  * Per-host compatibility toggles (strictly isolated; no engine-wide hacks without a plan).
* **Gmail Baseline:**

  * Login, inbox list, open thread, basic compose/send (limitations acceptable).
* **Instagram Baseline:**

  * Login, feed scroll, open post, view images/reels (upload optional).
* **YouTube Baseline:**

  * Open watch page, play video with audio, scrub, fullscreen (comments optional).
* **Deliverable:** A public “3-site demo build” with a short known-issues list and repeatable repro scripts.

---

## (Optional, after “viable”) Milestone 15: The Safety Engineer (Process Model + Sandbox Plan)

**Theme:** *Don’t Get Users Owned*
**Goal:** Begin a path toward real-world safety (even if still not “Firefox-grade”).

* **Site Isolation Plan:**

  * At minimum: per-tab process separation design doc + prototype.
* **Permissions Model:**

  * Camera/mic/geolocation off by default; explicit prompts later.
* **Hardening:**

  * Input sanitization audits, fuzzing hooks, CSP basics later.
* **Deliverable:** “We have a credible plan and prototype direction” (not full security parity).
