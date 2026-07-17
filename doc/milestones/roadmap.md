# Hummingbird Engine - Project Roadmap

This document outlines the high-level path from an empty repository to a browser that can
realistically be used — which we define concretely: **it can play a YouTube video and
scroll a TikTok feed**. If it can't do that, it's a weekend project, not a browser.

The philosophy is "Agile Iteration": strictly modular components, minimal memory
footprint, and progressive complexity. Current codebase modules live in `src/app`,
`src/core`, `src/engine`, `src/html`, `src/style`, `src/layout`, `src/renderer`, and
`src/platform`; keep Ports & Adapters intact (Core/HTML/Style/Layout never depend on
Platform).

**How this document relates to the others:**

* This roadmap is the high-level index. Each active/completed milestone has a detailed
  document (`doc/milestones/milestoneN.md`) with North Star deliverable, non-goals,
  critical path, and stories.
* `doc/TODOs.md` tracks the live backlog; completed items are archived in
  `doc/todo_archive/`.
* Status legend: **Done** (shipped, tag noted) / **Next** (detailed doc exists, work
  queued) / **Planned** (themed, target chosen, not yet broken into stories) /
  **Aspirational** (direction is committed, expect re-scoping before work starts).

> **Renumbering note (2026-07):** Milestones 6+ were restructured twice-over. First,
> "The Layouter" was pulled forward to M6 and "The Speedster" was merged into the
> Compositor. Second, the back half (M7+) was rebuilt as a **compatibility ladder**
> ending at YouTube/TikTok playback: the old Gmail/Instagram "viable demo" targets were
> dropped from the mainline (they can return via the target matrix after the endgame),
> and the old DOMster/Scripter++ pair was merged into one milestone since they shared a
> single demo. Old numbers appear in archived docs.

---

## The Compatibility Ladder (M6 → M16)

Every milestone from M6 onward is anchored to one real page that cannot work without
that milestone's functional expansion. A milestone is done when its proof target works,
not when its subsystem "looks finished."

| Milestone | Proof target | Capability it forces |
|---|---|---|
| M6 Layouter | DDG HTML homepage (visual parity) | flexbox + CSS compat + CI harness |
| M7 Programmable Document | TodoMVC (pinned snapshot) | DOM mutation + events + timers/microtasks |
| M8 Session Keeper | Hacker News: log in, post a comment | cookies + storage + session persistence |
| M9 Fetcher | HNPWA browsing live API data | fetch/XHR + CORS + HTTP cache |
| M10 Layouter II | Wikipedia desktop + old.reddit | fixed/sticky + scroll containers + z-index |
| M11 Inputter | Real login + rich text forms | focus, selection, clipboard, forms v2 |
| M12 Framework Gauntlet | m.youtube.com app shell (no video) | observers + History API + SPA navigation |
| M13 Media Engine I | TikTok embed plays one video | `<video>` progressive playback + audio |
| M14 Compositor | Feed scrolls at 60fps while video plays | layers + retained rendering |
| M15 Media Engine II | YouTube embed plays | MSE + adaptive streaming |
| M16 Endgame | YouTube watch page + TikTok feed | the long tail, per-host flags |

**Standing guardrails (start early, run forever):**

* **Missing-API telemetry (from M7):** once-per-page logging of every JS API / DOM
  property a proof target touches that we don't implement (the JS-era version of M4's
  unsupported-tag registry). This list *is* the backlog generator for M12 and M16.
* **Parser fuzzing (from M7):** libFuzzer harnesses over HtmlParser/CssParser in CI.
  Arena allocation + raw observer pointers + real-web input demands it long before any
  sandbox work.
* **WPT slices (from M6):** import small curated web-platform-tests subsets per area
  (start with a few dozen flexbox tests) so compatibility is a number that trends, not
  a feeling.
* **Perf budgets (tracked from M7, enforced from M14):** navigation→first-paint and
  keystroke→paint measured on the fixture set from M7 on; hard budgets become CI
  assertions when compositor work starts.
* **Extension API follow-through (M9):** the `browser.*` layer gets its second real
  consumer — a request-filtering hook and a built-in ad-block-lite extension. This also
  makes M13+ target pages materially lighter.

---

## Milestone 0: The Skeleton (Foundation) — Done (v0.1.0, Dec 2025)

**Theme:** *Architecture & Abstraction*
**Goal:** Establish the build system, core utilities, and platform abstractions without implementing browser logic yet.

* **Build System:** CMake setup separating `Core`, `Platform`, and `App`.
* **Memory Model:** `ArenaAllocator` for bulk DOM management.
* **Platform Abstraction:** `IWindow`, `IGraphicsContext`; implemented via SDL2 (window) + Blend2D (graphics).
* **Deliverable:** A "Hello Engine" executable that opens a window and clears the screen.

---

## Milestone 1: The Reader (HTML MVP) — Done (v0.1.0, Dec 2025)

**Theme:** *Data to Pixels (MVC Pattern)*
**Goal:** Render static HTML with hardcoded "User Agent" defaults and a working render tree.

* **HTML Pipeline:** Zero-copy tokenizer (`std::string_view`); DOM builder with open-element stack, void handling, and unsupported-tag logging.
* **Style Defaults:** Hardcoded mapping for headings, lists, links, code, pre, hr, blockquote, strong/em.
* **Layout Engine:** Block layout (vertical stacking, margin/padding), inline flow with greedy line breaking, BR/HR control objects.
* **Viewport:** Text wrapping and basic scrolling/camera offset.
* **Deliverable:** Visualize `mfw.html` legibly with headings, lists, inline links/code, and rules.

---

## Milestone 2: The Stylist (CSS & Rendering) — Done (v0.2.1, Jan 2026)

Detailed doc: [milestone2.md](milestone2.md)

**Theme:** *Separation of Concerns*
**Goal:** Replace hardcoded C++ styles with an external stylesheet pipeline.

* **CSS Parser:** Tokenize/parse selectors and declarations into arena-backed stylesheets.
* **Cascade:** Selector specificity and computed styles for every node.
* **Box Model Refinement:** Margins/padding/borders; `display: none` / `inline-block`.
* **Deliverable:** Pages render differently based on a `.css` file; real bullets for lists via pseudo-element logic.

---

## Milestone 3: The Navigator (The Web) — Done (v0.3.0, Jan 2026)

Detailed doc: [milestone3.md](milestone3.md)

**Theme:** *Connectivity & Interaction*
**Goal:** Connect the engine to the internet and handle page transitions.

* **Networking Layer:** `INetwork` via libcurl; IO thread with main-thread handoff (DOM/layout stay main-thread).
* **Engine Facade:** `Tab` owns the document lifecycle; `ResourceStore` state machine for HTML/CSS/images.
* **Interactivity:** Hit testing; clicking `<a>` triggers fetch and document swap.
* **UI:** URL bar with editing basics.
* **Deliverable:** A functional browser that can navigate from a homepage to an article.

---

## Milestone 4: The Brain (Scripting + Forms) — Done (v0.4.0, Feb 2026)

Detailed doc: [milestone4.md](milestone4.md)

**Theme:** *Logic & State*
**Goal:** Embed a JavaScript engine and make real search flows work without JS.

* **JS Engine:** QuickJS behind `IScriptEngine` (Core stays QuickJS-free); eval + error reporting, no exceptions across the boundary.
* **Bindings MVP:** `document.getElementById`, `textContent`, `console.log`; `click`/`load` event dispatch into JS.
* **Forms MVP:** `<form>/<input>/<button>` rendering, focus + text editing, GET submit → navigation.
* **Compatibility push:** selector coverage, entities, robust parsing, font-family mapping, warning registry.
* **Deliverable:** DDG HTML search executes end-to-end; internal JS demo page proves scripted DOM mutation with single-pass invalidation.

---

## Milestone 5: The Architect (Extensions & Tabs) — Done (v0.5.0, Jul 2026)

Detailed doc: [milestone5.md](milestone5.md) · Archive: [milestone5_done.md](../todo_archive/milestone5_done.md)

**Theme:** *Extensibility*
**Goal:** Create the "Browser OS" layer that manages tabs and extensions.

* **Tab Management:** `TabManager` with per-tab isolation (document/resources/scripts), minimal tab strip + shortcuts.
* **Extension Host MVP:** manifest v0, loader, long-lived background script runtime (one QuickJS context per extension), enable/disable lifecycle.
* **`browser.*` API v0:** tab lifecycle events, active-tab query, `insertCSS` with deterministic single-pass invalidation.
* **Built-in Dark Mode extension** injecting CSS across pages and navigations.
* **DDG completion track:** autofocus, input-submit parity, POST forms, hit-test reliability.
* **Architecture paydown:** package cycles broken (document/script, platform_api/geometry, dom/compute), `DocumentPipeline`/`ResourceLoader` decomposition.
* **Deliverable:** Multi-tab browser with a working Dark Mode extension; DDG HTML usable end-to-end.

---

## Milestone 6: The Layouter (Real-Page Layout Compatibility) — In progress

Detailed doc: [milestone6.md](milestone6.md)

**Theme:** *Modern Layout Primitives, Proven on a Real Page*
**Goal:** Implement the layout features real pages assume by default (flexbox first), close the remaining DDG visual gaps, and lock progress in with automated regression harnesses.

* **Flexbox v1:** row/column, alignment, basic flex factors — enough fidelity for form/logo centering on real pages.
* **CSS compatibility slice:** `clear`, border/radius longhands, vendor-prefix aliases, targeted legacy properties.
* **DDG parity:** homepage matches reference browser for the centered logo/search block.
* **Guardrails:** DDG snapshot regression harness in CI; automated dependency-cycle checks; first WPT flexbox slice.
* **Perf/memory hygiene (bounded):** batch resource updates, tab resource eviction, DOM budgets — only as far as real pages force it.
* **Proof target:** DDG HTML homepage renders like a reference browser, and CI fails if the type/submit/navigate flow or the layout regresses.

---

## Milestone 7: The Programmable Document (DOM + Events + Scheduling) — Planned

Detailed doc: [milestone7.md](milestone7.md)

**Theme:** *A Real, Mutable, Scriptable Page*
**Goal:** Merge the old "DOMster" and "Scripter++" milestones (they shared one demo) into a single expansion: JS can build and mutate UI, and time exists.

* **DOM Core:** mutation primitives (append/remove/replace) with strict arena ownership; `classList`, attributes, `dataset`; `querySelector`/`querySelectorAll`.
* **Event System v1:** `addEventListener`/`removeEventListener`, real event objects, capture/target/bubble propagation; pointer + keyboard routed Platform → Core via interfaces.
* **Scheduling:** `setTimeout`/`setInterval` task queue; microtask queue (Promise jobs) integrated with the main-loop tick; `requestAnimationFrame`. *(Microtasks are a hard prerequisite for M9's `fetch` — Promises don't work without them.)*
* **Invalidation Model:** mutations mark style/layout/paint dirty regions (no "rebuild everything").
* **Guardrails start:** missing-API telemetry; parser fuzzing in CI; GC/ownership rules documented (who owns what; arena-backed native nodes).
* **Proof target:** **TodoMVC (vanilla-JS build, pinned snapshot)** is fully usable — add, toggle, edit, filter, clear — with stable performance and no DOM corruption.

---

## Milestone 8: The Session Keeper (Cookies + Storage + Identity) — Planned

Detailed doc: [milestone8.md](milestone8.md)

**Theme:** *The Browser Remembers You*
**Goal:** Support the state layer every logged-in site assumes.

* **Cookies v1:** domain/path matching, Secure/HttpOnly, SameSite baseline (Lax/Strict); correct semantics across redirects; persistent cookie jar across restarts.
* **DOM Storage:** `localStorage`/`sessionStorage` (synchronous API, per-origin, persisted). Every SPA login flow touches this — it cannot wait for the fetch milestone.
* **Navigation plumbing:** POST hardening, redirect chains, basic error pages.
* **Proof target:** **Log into Hacker News, post a comment, restart the browser, still be logged in.**

---

## Milestone 9: The Fetcher (Fetch/XHR + CORS + Cache) — Planned

Detailed doc: [milestone9.md](milestone9.md) *(draft — revalidate at kickoff)*

**Theme:** *Pages That Talk to APIs*
**Goal:** JS-driven networking, done strictly.

* **Fetch/XHR v1:** request/response headers, redirects, buffering (streaming later); JSON round-trips.
* **CORS v1:** strict first; expand behind feature flags.
* **HTTP Cache v1:** ETag / If-None-Match, Cache-Control baseline, in-memory first.
* **Extension follow-through:** request-filtering hook in `browser.*` + built-in **ad-block-lite** extension — the API's second real consumer, and it makes every later target page lighter.
* **Proof target:** **HNPWA (Hacker News PWA)** browses live API data — lists, threads, pagination — rendered entirely from `fetch` responses.

---

## Milestone 10: The Layouter II (Fixed/Sticky + Scroll Containers + Stacking) — Planned

**Theme:** *Layout Features Deferred From M6*
**Goal:** The positioning/scrolling primitives that desktop-class pages assume.

* **Positioning:** `position: fixed` (non-negotiable), `sticky`, `absolute` correctness improvements.
* **Overflow + Scrolling v2:** real scroll containers (not just document scroll), clip/scroll offsets, hit-testing through scroll transforms.
* **Stacking Context v1:** z-index basics so headers/menus layer sanely.
* **History baseline:** back/forward/reload with a stable document lifecycle (no leaks, no dangling `string_view`) — needed before SPA navigation in M12.
* **Proof target:** **Wikipedia desktop and old.reddit** render with sticky/fixed headers, scrollable panes, and correct layering, and back/forward works through a browse session.

---

## Milestone 11: The Inputter (Forms v2, Focus, Selection, Clipboard) — Planned

**Theme:** *Human Interaction*
**Goal:** Make login and composing text practical (this is where "viable" often lives or dies).

* **Form Controls v2:** `<input type=text/password>`, `<textarea>`; value editing, selection, caret movement, copy/paste.
* **Focus System:** tab order, focus rings, keyboard routing.
* **Composition Plan:** Latin input "good enough" first; IME/composition events as a follow-up epic behind interfaces.
* **Text rendering reality check:** emoji + font-fallback chain baseline (comment sections are full of them; full shaping/bidi stays deferred).
* **Proof target:** **Log into a major site (e.g., GitHub or Wikipedia) reliably**; write, select, copy/paste, and edit multi-line text without glitches.

---

## Milestone 12: The Framework Gauntlet (Observers + History API + SPA Navigation) — Aspirational

**Theme:** *Survive a Production Framework*
**Goal:** The API surface that framework-generated apps (Polymer/React/Vue class) assume, driven directly by missing-API telemetry from the proof target.

* **History API:** `pushState`/`replaceState`/`popstate`; SPA route changes without document teardown.
* **Observer APIs:** `MutationObserver`; `IntersectionObserver` (feeds/lazy-loading depend on it); `ResizeObserver` (stub → real as telemetry demands).
* **Custom Elements:** `customElements.define()` upgrade path with lifecycle callbacks (YouTube is Polymer — this is not optional for the endgame).
* **Long-tail bindings:** whatever the telemetry log says the target shell touches (`getBoundingClientRect`, `matchMedia`, `navigator.*` basics, etc.).
* **Proof target:** **m.youtube.com renders its app shell** — thumbnails, navigation between pages — with video playback explicitly out of scope.

---

## Milestone 13: The Media Engine I (Progressive Video + Audio) — Aspirational

**Theme:** *Pixels That Move, Sound That Plays*
**Goal:** Functional `<video>`/`<audio>` for progressive (non-adaptive) sources. TikTok serves progressive MP4, which is why it comes a full milestone before YouTube.

* **Decode adapters:** `IVideoDecoder`/`IAudioOutput` behind Platform (ffmpeg/libav + SDL audio are the obvious backends); Core never sees codec types.
* **`<video>` v1:** frame presentation into the render tree, play/pause/seek on progressive MP4, volume, basic controls, fullscreen (platform adapter).
* **A/V sync:** audio-clock-driven; frame drops over stalls.
* **Image Pipeline v2 leftovers:** animated formats, caching, correct sizing under flex.
* **Proof target:** **A TikTok embed page plays a single video with sound**, plus a local fixture page for deterministic CI.

---

## Milestone 14: The Compositor (Layers + Retained Rendering) — Aspirational

**Theme:** *Don't Repaint the World*
**Goal:** Smooth scrolling and cheap video presentation. (Absorbs the old "Speedster" milestone.)

* **Retained Display List:** build once, diff on invalidation; stable IDs for render nodes.
* **Layer Tree:** scroll layers move as textures; video gets its own layer so playback doesn't force page repaints.
* **Raster Strategy:** DOM/layout stay main-thread; optional raster thread behind Platform.
* **Perf budgets become CI assertions:** navigation→first-paint, keystroke→paint, scroll frame time on the fixture set.
* **Proof target:** **A feed-scroll harness (TikTok-like snapshot) scrolls at 60fps-class smoothness while a video keeps playing.**

---

## Milestone 15: The Media Engine II (MSE + Adaptive Streaming) — Aspirational

**Theme:** *Streaming Like the Real Ones*
**Goal:** Media Source Extensions — the way YouTube actually delivers video.

* **MSE v1:** `MediaSource`/`SourceBuffer`, append/remove, buffered ranges, the buffering state machine.
* **Byte-range fetch integration** with the M9 network stack; adaptive quality switching (manual first, automatic later).
* **Workers decision:** Web Worker v1 or a documented stub — whatever the YouTube player build requires (telemetry decides).
* **Explicit non-goal:** no EME/DRM. Standard videos only; DRM content will not play, and that's an accepted limitation.
* **Proof target:** **A YouTube embed page plays a video** with scrub and at least one resolution switch.

---

## Milestone 16: The Endgame (YouTube Watch Page + TikTok Feed) — Aspirational

**Theme:** *A Browser Somebody Could Actually Use*
**Goal:** Close the long tail on the two endgame targets, driven by telemetry, guarded by per-host flags.

* **Target Matrix + Feature Flags:** per-host compatibility toggles, strictly isolated; no engine-wide hacks. (Other sites — Gmail, Instagram, anything — can be added to the matrix *after* the endgame; they are explicitly not on the critical path.)
* **YouTube baseline:** open a watch page on youtube.com/m.youtube.com → video plays with audio, scrub, fullscreen. Comments/login optional.
* **TikTok baseline:** open tiktok.com → scroll the feed, videos autoplay with sound. Login/upload optional.
* **iframe v1 (if forced):** same-process, no sandbox — only if the targets require it for embeds/players; otherwise it stays deferred.
* **Known risk, stated honestly:** both sites run anti-bot detection (TLS/JS fingerprinting) that may block an unknown engine regardless of technical capability. Mitigations: develop against pinned snapshots + embed endpoints first, keep the live-site check a manual gate, and decide UA policy when we get there.
* **Deliverable:** a public demo build + short known-issues list + repeatable repro scripts proving both baselines.

---

## (Optional, post-endgame) Milestone 17: The Safety Engineer (Process Model + Sandbox Plan) — Aspirational

**Theme:** *Don't Get Users Owned*
**Goal:** Begin a path toward real-world safety (parser fuzzing already runs from M7; this is the structural half).

* **Site Isolation Plan:** per-tab process separation design doc + prototype.
* **Permissions Model:** camera/mic/geolocation off by default.
* **Hardening:** input sanitization audits, CSP basics.
* **Deliverable:** "We have a credible plan and prototype direction" (not full security parity).

---

## Explicitly deferred subsystems (tracked so they don't vanish)

These are known gaps with no milestone of their own; each is pulled in only when a proof
target forces it, and the decision (build/stub/reject) gets recorded here:

* **iframes** — M16 if forced by embeds/players.
* **Service workers** — stub/ignore until a target breaks without them (YouTube/TikTok load without on first visit).
* **Canvas/WebGL** — not needed for the endgame playback path; revisit post-M16.
* **Full text shaping/bidi (HarfBuzz-class)** — emoji + fallback baseline lands in M11; real shaping is post-endgame.
* **EME/DRM** — rejected for the roadmap horizon.
* **Accessibility beyond landmark roles** — revisit post-endgame; keep roles working meanwhile.
