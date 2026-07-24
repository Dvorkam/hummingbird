# Changelog

All notable changes to this project will be documented in this file.

## [0.8.0] - Unreleased

Milestone 8 — "The Session Keeper": persistent browser state and the navigation
plumbing needed for a real authenticated session. The implementation is complete;
the release is pending.

### Added

- **Cookie engine v1**: engine-owned parsing, domain/path matching, expiry,
  `Secure`, `HttpOnly`, SameSite Lax/Strict policy, and persistent-cookie storage.
- **`document.cookie`** reads and writes through the engine cookie jar while hiding
  `HttpOnly` cookies from scripts.
- **DOM Storage**: persistent, per-origin `localStorage` with a roughly 5 MB quota
  and `QuotaExceededError`, plus isolated per-tab `sessionStorage` that is never
  written to disk.
- **Engine-owned redirect chains** with hop and loop limits, intermediate-hop cookie
  processing, and browser-shaped 301/302/303/307/308 method semantics.
- **Navigation request context**: `Referer` under the browser-default
  `strict-origin-when-cross-origin` policy and `Origin` on POST requests.
- **Multiline `<textarea>` MVP** for typing, editing with Backspace, newlines, and
  form serialization in the Hacker News comment flow.
- **Per-origin compatibility mode** (`Ctrl+Shift+U`): an explicit, persisted
  Chrome-shaped User-Agent override for servers that reject Hummingbird's identity.
  `Sec-CH-UA` continues to identify Hummingbird, the mode is never enabled
  automatically, and toggling it never silently replays a POST.
- **Stable network-error pages** for unreachable sites and redirect failures, with
  the failed URL, a human-readable reason, reload guidance, and a retry link.
- M8 built-in demonstrations for cookies, storage, textarea submission, sessions,
  and network errors.
- **Modern-layout resilience** (M8.5), so pages built with contemporary CSS degrade
  gracefully instead of breaking apart:
  - `object-fit` (`fill`, `contain`, `cover`, `none`, `scale-down`) when a replaced
    element's box does not match the media's aspect ratio. `object-position` is not
    implemented yet; content is centred.
  - Paint-time clipping for `overflow: hidden` and `overflow: clip`, so an oversized
    child no longer paints across the rest of the page. Scrolling and scrollbars
    remain out of scope.
  - The `rem` unit, resolved against the document root's font size, alongside `em`
    against the element's own.
- A built-in layout demonstration page covering relative sizing, `object-fit`, and
  overflow clipping.

### Improved

- HTTP request and response headers now cross the network interface without losing
  repeated fields such as `Set-Cookie`.
- Reload is available through `F5` and `Ctrl+R`.
- Table compatibility for nested percentage widths and legacy `bgcolor` attributes.
- Legacy `<b>` and `<i>` elements now receive their expected bold and italic styles.

### Fixed

- Enforced SameSite policy on top-level navigations as well as subresources,
  closing a cross-site request-forgery policy gap.
- Expired cookies are no longer written to the persistent jar.
- `<input type="hidden">` no longer produces a layout box, and the legacy `size`
  attribute is no longer misinterpreted as a font size.
- `file://` navigation is rejected at the page-controlled URL boundary.
- Images and inline SVG sized with a percentage or a font-relative length now
  follow that length instead of ballooning to the media's intrinsic size, keep
  their aspect ratio when only one axis is given, and stay correct under
  `max-width`/`box-sizing` clamping.
- `font-family: var(--name)` resolves the variable instead of being treated as a
  literal family name.
- Repeated compatibility warnings are now summarised per document rather than
  flooding the log.

### Guardrails and documentation

- Added a CI-runnable HN-shaped login/comment fixture that proves login POST,
  `Set-Cookie`, authenticated comment POST, jar save/load, restart-safe sessions,
  logout, and cookie clearing through the real tab pipeline.
- Added RFC/HTML conformance registers and an interactive page-pipeline explorer.

## [0.7.0] - 2026-07-20

Milestone 7 — "The Programmable Document": a mutable DOM, browser-shaped events,
and scheduling. A pinned vanilla-JS TodoMVC snapshot is fully interactive, and the
Hacker News comment-collapse script works on the supported item-page flow.

### Added

- External scripts load and execute in document order through the resource pipeline.
- **DOM mutation and traversal**: element/text creation, append/insert/remove/replace,
  sibling and child traversal, attributes, `classList`, `dataset`, and `innerHTML`.
- **DOM queries**: `querySelector`, `querySelectorAll`, `matches`, `closest`, and
  `getElementsByClassName`/`getElementsByTagName` over the supported selector subset.
- **Event system v1**: `EventTarget`, listener registration/removal, event objects,
  capture/target/bubble propagation, cancellation, and propagation controls.
- Pointer, keyboard, focus, input, change, and submit events routed through the DOM
  event pipeline; canceling click/keydown/submit suppresses the matching default
  action.
- **Scheduling**: `setTimeout`, `setInterval`, cancellation, Promise microtask
  checkpoints, and `requestAnimationFrame`/`cancelAnimationFrame`.
- Fragment navigation, `hashchange`, `window.location.hash`, and synchronization of
  script-driven hash changes with tab history and the URL bar.
- Interactive checkbox rendering and behavior, plus the form-control JS surface for
  `value`, `checked`, `disabled`, `focus()`, and `blur()`.
- Per-tab back/forward history and persistent bookmarks with `about:bookmarks`.
- Missing-API telemetry with fail-soft stubs for selected unimplemented APIs.

### Improved

- DOM changes within one script task are coalesced into one style/layout pass.
- Keyboard events report digits, space, punctuation, and Tab through `key`/`code`.
- Table-cell intrinsic sizing now follows content instead of block stretch, fixing
  the Hacker News item-page layout needed by the secondary proof target.

### Fixed

- Detached arena-backed DOM nodes remain valid for in-flight event dispatch and
  `innerHTML` replacement, preventing wrapper and event-target corruption.
- Re-entrant script dispatch preserves the outer mutation epoch and teardown order.
- Fragment/query links replace the base fragment instead of appending to it.
- DOM/event correctness and shrink-to-fit layout issues found during the pre-release
  review.

### Guardrails

- Added the pinned TodoMVC end-to-end fixture and a focused Hacker News
  collapse/expand reproduction.
- Added HTML/CSS parser libFuzzer harnesses, seed corpora, AddressSanitizer
  instrumentation, and a bounded CI fuzzing job.
- Documented native/JavaScript ownership, wrapper identity, detach-never-free arena
  semantics, and navigation teardown rules.

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
