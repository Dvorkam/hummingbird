### **Milestone 3: The Navigator (Backlog)**

**Theme:** Document lifecycle + resource loading + navigation.
**Goal:** Turn the current “single document driven by `BrowserApp`” into an engine-owned document container that can navigate, load subresources (CSS/images), and update deterministically as resources arrive.

**Constraints (non-negotiable):**

* **Main thread owns the world:** DOM/style/layout/paint stay on the main thread (see `doc/coding_constitution.md`).
* **Ports & Adapters stays intact:** Core/Html/Style/Layout/Renderer do not include Platform headers.
* **Deterministic updates:** Late-arriving resources trigger controlled invalidation (style/layout/paint) without frame-by-frame rebuilds.

---

#### **Epic 3.1: Engine Facade + Document Lifecycle (Tab)**

**Goal:** Introduce an engine-owned “document container” (e.g., `Hummingbird::Engine::Tab`) that owns the DOM arena, stylesheets, render tree, and pipeline stages. The app becomes wiring + chrome.

* **Story 3.1.1: Engine Container Skeleton**
* **As a** developer,
* **I want** an engine object that encapsulates the current pipeline components (`ArenaAllocator`, HTML parse, `StyleEngine`, `TreeBuilder`, layout, `Painter`) behind a small API.
* **Acceptance:** A single entrypoint like `Tab::Navigate(url)` and `Tab::Tick()` exists; the app does not own DOM or “pending HTML buffers”.

* **Story 3.1.2: Document State Reset**
* **As a** memory-safety engineer,
* **I want** navigation to tear down the old document and reset the arena in one place.
* **Acceptance:** Navigating replaces the DOM/render tree and calls `ArenaAllocator::reset()` exactly once per document swap.

---

#### **Epic 3.2: Resource Loading Pipeline (HTML + Subresources)**

**Goal:** A unified resource system that can fetch: main HTML, linked CSS, images (favicons optional). Build on the existing resource boundary (`IResourceProvider`) and extend it to be URL/network-backed.

* **Story 3.2.1: Resource Store + State Machine**
* **As a** platform-agnostic engine,
* **I want** a `ResourceStore` that tracks `Requested → Loading → Ready/Failed` and exposes immutable resource views to style/layout.
* **Acceptance:** The engine can request a URL and later observe a stable “ready body bytes/text” without races.

* **Story 3.2.2: Resource Identities**
* **As a** developer,
* **I want** resources keyed by canonicalized URL + type (`Document`, `Stylesheet`, `Image`) so dedup/caching is possible later.
* **Acceptance:** Re-requesting the same stylesheet URL does not create duplicate in-flight work.

---

#### **Epic 3.3: URL Canonicalization + Relative URL Resolution**

**Goal:** Reliable resolution of relative URLs (`/x`, `../y`, `images/a.png`) and basic normalization (scheme/host casing/default ports).

* **Story 3.3.1: URL Resolution MVP**
* **As a** browser engine,
* **I want** to resolve relative URLs against the document URL.
* **Acceptance:** `href="styles.css"` and `src="/images/a.png"` become correct absolute URLs for resource requests.

* **Story 3.3.2: Input Normalization**
* **As a** user,
* **I want** `example.com` to be treated as `https://example.com` (or a consistent fallback strategy).
* **Acceptance:** Typing a bare hostname navigates without requiring the scheme.

---

#### **Epic 3.4: External Stylesheet Integration Over HTTP (Incremental Styling)**

**Goal:** Make `<link rel="stylesheet">` fetched via network affect pixels, and do so deterministically as sheets arrive.

* **Story 3.4.1: Link Stylesheet Fetch**
* **As a** style pipeline,
* **I want** discovered stylesheet links (already extracted by HTML parsing) to be fetched through the engine resource pipeline.
* **Acceptance:** A document with `<link rel="stylesheet" href="…">` triggers a request and eventually produces a stylesheet text resource.

* **Story 3.4.2: Author Merge Order**
* **As a** style engine,
* **I want** author sources merged in the correct order: `UA → <link> sheets (doc order) → <style> blocks (doc order)`.
* **Acceptance:** If two rules tie on specificity, later sources win exactly as expected.

* **Story 3.4.3: Incremental Invalidation**
* **As a** renderer,
* **I want** late-arriving stylesheets to trigger `style → layout → paint` invalidation once per completed sheet.
* **Acceptance:** Pages update deterministically when CSS loads; no “rebuild everything every frame” behavior.

---

#### **Epic 3.5: Image Pipeline MVP (Fetch + Decode + Layout + Paint)**

**Goal:** Turn `<img>` from a placeholder into real pixels on screen.

* **Story 3.5.1: Image Fetch**
* **As a** resource system,
* **I want** `<img src>` to request the bytes through the resource pipeline.
* **Acceptance:** Image URLs appear in `ResourceStore` and transition to `Ready/Failed`.

* **Story 3.5.2: Decode Behind an Adapter**
* **As a** platform maintainer,
* **I want** image decoding hidden behind an interface (e.g., `IImageDecoder`) so Core/Layout stays platform-agnostic.
* **Acceptance:** Layout/Renderer operate on an `ImageBitmap`-like value (dimensions + pixel buffer handle), not on platform types.

* **Story 3.5.3: Stable Layout Reservation**
* **As a** user,
* **I want** images to reserve space early using intrinsic/declared dimensions so the layout doesn’t jump unpredictably.
* **Acceptance:** If size is known (attrs or decoded), the box reserves it; pixels swap in when ready.

---

#### **Epic 3.6: Networking Concurrency Model (IO Thread) + Main-Thread Handoff**

**Goal:** Keep DOM/style/layout/paint strictly main-thread, but make IO concurrent and safe.

* **Story 3.6.1: IO-Only Work**
* **As a** engine architect,
* **I want** the IO side to perform only network + byte acquisition (no DOM touching, no layout, no painting).
* **Acceptance:** Resource completion delivers immutable bytes/messages to the main thread.

* **Story 3.6.2: Completion Queue**
* **As a** performance engineer,
* **I want** resource completions delivered through a minimal queue (lock-free-ish or minimal mutex) consumed on the main thread.
* **Acceptance:** Navigation does not stall while resources download; resource completions are processed during `Tab::Tick()`.

* **Story 3.6.3: Navigation Race Protection**
* **As a** correctness engineer,
* **I want** stale responses from prior navigations to be ignored (current code uses a navigation id).
* **Acceptance:** Late responses for an old page never mutate the new document.

---

#### **Epic 3.7: Hit Testing + Link Activation (Navigation by Clicking)**

**Goal:** Interactivity sufficient for real navigation.

* **Story 3.7.1: Hit Test API**
* **As a** UI layer,
* **I want** a hit-test against the render tree that returns the DOM node / link target under a point.
* **Acceptance:** Clicking a rendered anchor returns an `href` (if present).

* **Story 3.7.2: Link Navigation**
* **As a** user,
* **I want** clicking a link to navigate to the resolved URL via the engine facade.
* **Acceptance:** Clicking links triggers `Tab::Navigate(resolved_url)` and swaps documents.

---

#### **Epic 3.8: URL Bar “Minimally Solid” UX**

**Goal:** Keep it small, but remove paper cuts.

* **Story 3.8.1: Editing Basics**
* **As a** user,
* **I want** insertion/backspace/delete, left/right, home/end in the URL bar.
* **Acceptance:** The URL bar feels like an address bar, not a debug text field.

* **Story 3.8.2: Submit / Focus Behavior**
* **As a** user,
* **I want** `Ctrl+L` focus, `Esc` unfocus, `Enter` navigate to remain consistent during navigation and loads.
* **Acceptance:** Focus doesn’t get “stuck” after navigation; no crashes on rapid keypresses.

---

#### **Epic 3.9: Rendering Performance Hygiene for Navigation**

**Goal:** Add enough instrumentation and culling to make navigation + incremental loads observable and not regress into constant full rebuilds.

* **Story 3.9.1: Extend Paint Instrumentation**
* **As a** performance engineer,
* **I want** timing numbers for key phases (resource processing, style, layout, paint) around navigation and incremental updates.
* **Acceptance:** Logs show stable per-phase timings; regressions are visible.

* **Story 3.9.2: Viewport Culling Consistency**
* **As a** renderer,
* **I want** viewport culling to apply consistently (including debug overlays) so large pages don’t paint offscreen content.
* **Acceptance:** Loading a large page does not degrade into painting everything every frame.

---

#### **Epic 3.10: Headless Engine Harness + Navigation/Resource Tests**

**Goal:** Add an engine-level test harness that can run the full pipeline without a window and simulate resource delivery.

* **Story 3.10.1: Headless Tab Harness**
* **As a** test author,
* **I want** a headless mode that runs parse/style/layout without SDL.
* **Acceptance:** Tests can render fixtures without requiring a window or GPU context.

* **Story 3.10.2: Navigation + Resource Tests**
* **As a** maintainer,
* **I want** regression tests for URL resolution, stylesheet merge order, resource state transitions, and navigation swaps.
* **Acceptance:** Tests cover:
  * URL canonicalization + relative resolution
  * Stylesheet discovery + merge order
  * Resource state machine (`Requested/Loading/Ready/Failed`)
  * Navigation swapping (old arena freed/reset, new document created)

