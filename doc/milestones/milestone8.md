> **Status: Complete** — implementation and proof gates finished 2026-07-22;
> planned for v0.8.0.
>
> **M8.5 addendum in progress (2026-07-23)** — a small modern-layout-resilience
> slice added after the seznam.cz investigation; see the "Milestone 8.5 Addendum"
> section at the end. Does not gate the v0.8.0 release.

## Kickoff Scope Validation (2026-07-20)

The pre-written network-seam scopes were checked against the codebase as the
header note asked. Two findings change the story list:

1. **There is no HTTP header plumbing at all.** `NetworkResponse`
   (`core/platform_api/INetwork.h`) carries only url/effective_url/body/status/
   error, and `NetworkRequestOptions` carries only `allow_insecure` and
   `content_type`. `CurlNetwork` extracts `CURLINFO_CONTENT_TYPE` and nothing
   else — there is no header callback. A cookie jar cannot read `Set-Cookie` or
   send `Cookie` until both directions exist, so **story 8.1.0 is inserted ahead
   of 8.1.1** to carry that plumbing.

2. **libcurl follows redirects internally** (`CURLOPT_FOLLOWLOCATION = 1L` in
   `apply_common_curl_options`), so the engine never observes intermediate hops.
   That makes "cookies set mid-chain apply to subsequent hops; site context is
   recomputed per hop" (8.1.3) and hop-limit/loop-detection/method-rewriting
   (8.3.1) both unimplementable as written. Taking ownership of the redirect loop
   is 8.3.1's job, and 8.1.3 depends on it — so **8.3.1 now runs before 8.1.3**
   in the execution order rather than after.

Everything else (storage, `document.cookie`, the login harness) validated as
written.

### Story 8.1.0: HTTP Header Plumbing Across The Network Seam

* **Goal:** let the engine read response headers and set request headers, so
  cookie policy can live in the engine rather than in libcurl.
* **Scope:** response headers on `NetworkResponse` and request headers on
  `NetworkRequestOptions`; a curl header callback; matching support in
  `StubNetwork` so fixtures can drive `Set-Cookie`. Header names are matched
  case-insensitively and repeated headers (`Set-Cookie` especially) must be kept
  as a list, not collapsed into one value.
* **Acceptance:** a caller can set request headers that reach the backend and can
  read response headers off `NetworkResponse`, with repeated `Set-Cookie` fields
  preserved individually.
* **Tests:** header-collection unit tests (case-insensitive lookup, repeated
  fields, raw-line parsing incl. the status/blank lines curl emits) + a seam
  round trip through the network fakes. **The libcurl path is not locally
  verifiable** (no network in the dev sandbox); it is covered by review and by
  the live-HN manual gate, same caveat as the M7 fuzzing job.
* **Non-goals:** no cookie semantics here — this story only moves bytes. Wiring
  headers into `ResourceLoader`'s document/subresource requests belongs to 8.1.1,
  where there is a jar to wire them to.

## Milestone 8 North Star Deliverable

**Before (after Milestone 7):**

* JS-driven pages work, but the browser is amnesiac: no cookie storage, so every
  navigation is a first visit and no login survives a single page load.
* No `localStorage`/`sessionStorage` — the state layer virtually every logged-in site
  assumes is absent.
* Redirect handling exists but has no cookie semantics and no hardening (limits, loop
  detection, method rewriting).

**After (Milestone 8 done):**

* **Cookie engine v1**: Set-Cookie parsing, domain/path matching, Secure/HttpOnly,
  SameSite baseline, correct behavior across redirects, persisted to disk.
* **DOM Storage**: `localStorage` (persistent, per-origin) and `sessionStorage`
  (per-tab lifetime).
* **`document.cookie`** JS binding with HttpOnly filtering.
* **HN textarea MVP**: a deliberately narrow multiline control sufficient to type
  and submit a Hacker News comment; the rest of textarea editing/form semantics is
  explicitly deferred to M11.
* **Hardened navigation plumbing**: redirect limits/loop detection, 301/302/303/307/308
  method semantics, basic network-error pages.
* **Proof target:** **log into Hacker News, post a comment, restart the browser, and
  still be logged in.**

---

## Non-Goals (keep the blast radius controlled)

* No fetch/XHR (M9) — cookies/storage land first precisely so M9 requests inherit
  correct semantics.
* No IndexedDB, no Cache API, no storage events across tabs.
* No cookie manager UI, no third-party-cookie blocking policy, no partitioning.
* No encryption-at-rest for the cookie jar/storage files (plain per-profile files;
  note the limitation).
* No `__Secure-`/`__Host-` prefix enforcement beyond parsing (record as follow-up if
  a target site forces it).
* The textarea slice is not Forms v2: no mouse/keyboard selection, word/line
  navigation, clipboard, `selectionStart`/`selectionEnd`, `defaultValue`, form
  reset, CSS `resize`, internal scrolling, or general DOM/API parity. Those
  extensions are tracked as M11 follow-ups in `doc/TODOs.md`.

---

## Critical Path (what must land for the North Star)

**Must-have**

* Cookie jar with matching/attribute policy (8.1.1, 8.1.2) wired into every engine
  request (document, subresource, POST).
* HN textarea MVP (8.0.1), so the proof target has a real comment payload rather
  than only session plumbing.
* Redirect cookie semantics + chain hardening (8.1.3, 8.3.1).
* Jar persistence across restarts (8.1.4).
* `document.cookie` binding (8.1.5) — HN's login flow is form-POST, but session
  checks touch it.
* HN login/comment harness against a local fixture server (8.4.1).

**Generality, not strictly North Star** (corrected at implementation time, 2026-07-22)

* `localStorage` with persistence (8.2.1, 8.2.2). The doc originally listed this
  as a North Star must-have, but HN's session is entirely cookie-based, so the
  proof target does not require it. It is here so the browser stops *silently
  losing* client-side state (the 7.5.2 fail-soft stub discards writes), which
  matters far more for the M12 framework sites than for HN. Built because it is
  cheap once the tested core exists, not because the North Star needs it.

**Nice-to-have (if schedule allows)**

* `sessionStorage` per-tab semantics (8.2.3) — cheap once 8.2.1 exists.
* Network-error pages (8.3.2).

---

## Milestone 8 Done When

* The HN flow passes manually against the live site: log in → post comment → restart
  browser → still logged in.
* The same flow passes in CI against a pinned-fixture local server (login form,
  Set-Cookie, authenticated POST, session check).
* Cookie matching/attribute tests pass (domain/path, expiry, Secure, HttpOnly,
  SameSite Lax/Strict, redirect behavior).
* `localStorage` data survives restart; `sessionStorage` dies with the tab; quotas
  enforced.
* No cookie or storage state leaks across tabs beyond spec (shared jar per profile,
  per-origin storage; per-tab sessionStorage).

---

## Stories

### 8.0 - HN Comment Surface (bounded Forms v2 pull-forward)

* **Story 8.0.1: Textarea MVP For HN Comments**
* **Goal:** make a native `<textarea>` usable for the single HN comment workflow:
  focus it, type ordinary text including newlines, edit with Backspace, and submit
  its current value in an urlencoded form POST.
* **Scope:** recognize `<textarea>` as a form-associated, focusable control; reuse
  the existing input editing/event path where it fits; give it a simple visible
  multiline box (with a conservative `rows`-based default height); insert typed
  text, Backspace, and `Enter` as a newline; serialize a named textarea's live
  value as `name=value` alongside inputs when its enclosing form is submitted.
  Clicking HN's submit control remains the submission path — `Enter` must add a
  newline, not submit. Add an M8 demo-site example showing a multiline comment
  field and its submitted payload.
* **Acceptance:** on HN's comment form, a user can type a short multi-line comment
  and submit it; the request contains the text in the textarea's named field;
  existing text-input behavior remains unchanged.
* **Tests:** control editing + form-serialization tests (including newline payload),
  input-event/focus regression tests, and the M8 local login/comment fixture.
* **Explicit follow-through:** this is intentionally not a general textarea. M11
  owns the editing/selection, JS/form, and sizing/scrolling extensions recorded in
  `doc/TODOs.md` as `T-FORM-TEXTAREA-*-1`.

### 8.1 - Cookie Engine v1

* **Story 8.1.1: Cookie Jar + Matching**
* **Goal:** parse `Set-Cookie`, store cookies, and attach them to requests via
  domain/path matching with expiry/max-age handling.
* **Scope:** engine-owned jar (single profile) behind the network seam; libcurl's own
  cookie engine stays off — the jar is engine-owned so policy is testable.
* **Acceptance:** a server-set cookie is returned on the next matching request and
  not on non-matching domain/path.
* **Tests:** jar unit tests (matching matrix, expiry).

* **Story 8.1.2: Attribute Policy (Secure/HttpOnly/SameSite)**
* **Goal:** honor `Secure` (HTTPS-only), `HttpOnly` (hidden from JS), and SameSite
  Lax-by-default with Strict supported.
* **Scope:** jar policy + request-context plumbing (top-level vs subresource,
  same-site computation).
* **Acceptance:** attribute matrix behaves per baseline spec; Lax cookies attach on
  top-level navigation but not cross-site subresources.
* **Tests:** policy unit tests.

* **Story 8.1.3: Redirect Cookie Semantics**
* **Goal:** cookies set mid-chain apply to subsequent hops; site context is
  recomputed per hop.
* **Scope:** ResourceLoader redirect path + jar integration.
* **Acceptance:** a login POST that 302s with `Set-Cookie` lands authenticated.
* **Tests:** redirect-chain integration tests (fixture server).

* **Story 8.1.4: Cookie Jar Persistence**
* **Goal:** persist non-session cookies to a per-profile file; load at startup;
  session cookies die with the process.
* **Scope:** serialization + startup/shutdown hooks; corrupt-file recovery (start
  empty, log).
* **Acceptance:** restart preserves persistent cookies exactly; expired ones are
  purged on load.
* **Tests:** persistence round-trip tests.

* **Story 8.1.5: document.cookie Binding**
* **Goal:** JS read/write of non-HttpOnly cookies for the document's origin.
* **Scope:** JS binding + jar filter path.
* **Acceptance:** JS sees its own cookies but never HttpOnly ones; writes obey
  attribute parsing.
* **Tests:** binding tests.

### 8.2 - DOM Storage

* **Story 8.2.1: Storage Backing Store**
* **Goal:** per-origin key/value store with quota (small, e.g. 5 MB/origin) and
  deterministic eviction refusal (throw on quota, per spec).
* **Scope:** engine-owned store, heap-side (not arena — outlives documents).
* **Acceptance:** get/set/remove/clear/length/key behave; quota exceeded throws.
* **Tests:** store unit tests.

* **Story 8.2.2: localStorage Binding + Persistence**
* **Goal:** `window.localStorage` bound to the per-origin store, persisted to disk
  per profile.
* **Scope:** JS binding + persistence (load lazily per origin; write-through or
  flush-on-tick — pick and document).
* **Acceptance:** values survive restart; origins are isolated.
* **Tests:** binding + persistence tests.

* **Story 8.2.3: sessionStorage (per-tab)**
* **Goal:** same API, lifetime bound to the tab, not persisted.
* **Scope:** per-tab store instance; navigation within the tab preserves it, closing
  the tab drops it.
* **Acceptance:** two tabs on the same origin see different sessionStorage.
* **Tests:** tab-lifecycle storage tests.

### 8.3 - Navigation Plumbing

* **Story 8.3.1: Redirect Chain Hardening**
* **Goal:** hop limit, loop detection, and method semantics (303 → GET, 307/308
  preserve method/body).
* **Scope:** ResourceLoader redirect handling.
* **Acceptance:** pathological chains terminate with a clear error; POST → 302 → GET
  behaves like reference browsers.
* **Tests:** redirect matrix tests (fixture server).

* **Story 8.3.2: Network Error Pages**
* **Goal:** DNS failure/refused/timeout/TLS error render a stable internal error page
  with a retry affordance instead of a blank document.
* **Scope:** DocumentPipeline error path + internal page template.
* **Acceptance:** killing the fixture server mid-navigation shows the error page;
  retry works.
* **Tests:** engine error-path tests.

### 8.4 - Guardrails

* **Story 8.4.1: Login-Flow Harness (T-HN-E2E-1)**
* **Goal:** CI-runnable end-to-end session test: local fixture server serving an
  HN-shaped login form → Set-Cookie → authenticated POST → restart → session check.
* **Scope:** fixture server + headless harness (pattern of T-DDG-E2E-1/T-TODOMVC-E2E-1);
  live-HN run stays a manual gate.
* **Acceptance:** CI fails if any link in the session chain regresses.
* **Tests:** integration harness.

---

## Execution Order Checklist

P0: Cookies (North Star)
- [x] 8.0.1: Textarea MVP For HN Comments
- [x] 8.1.0: HTTP Header Plumbing Across The Network Seam *(inserted at kickoff; prerequisite for 8.1.1)*
- [x] 8.1.1: Cookie Jar + Matching
- [x] 8.1.2: Attribute Policy (Secure/HttpOnly/SameSite) *(incl. T-COOKIE-NAV-INITIATOR-1: SameSite now enforced for both subresources and navigations)*
- [x] 8.3.1: Redirect Chain Hardening *(moved ahead of 8.1.3: it takes ownership of the redirect loop from libcurl, which 8.1.3 needs)*
- [x] 8.1.3: Redirect Cookie Semantics *(cookies set mid-chain ride the remaining hops; both the Cookie header and the SameSite context are recomputed per hop)*
- [x] 8.1.4: Cookie Jar Persistence
- [x] 8.1.5: document.cookie Binding

P0: Storage (North Star)
- [x] 8.2.1: Storage Backing Store
- [x] 8.2.2: localStorage Binding + Persistence *(methods + length + QuotaExceededError; `localStorage.foo` dot/bracket access deferred to T-STORAGE-DOT-ACCESS-1)*

P0: Guardrails
- [x] 8.4.1: Login-Flow Harness *(tests/engine/LoginFlow.test.cpp — an in-process fixture INetwork serves an HN-shaped flow; the test drives the real Tab pipeline through anonymous → credentialed login POST → persistent Set-Cookie → authenticated page → jar save/load "restart" → session check → logout → cleared, plus a comment POST gated on the session. Satisfies the "Done When" CI criterion.)*

P1: If Schedule Allows
- [x] 8.2.3: sessionStorage (per-tab) *(StorageKind discriminator through the script-host seam + QuickJS function magic; a per-tab in-memory StorageArea map on the Tab — never persisted, dropped with the tab. Demo at example.dev/session. Tests: SessionStorage.test.cpp — separate namespace from localStorage, survives in-tab navigation, isolated between tabs.)*
- [x] 8.3.2: Network Error Pages *(NetworkErrorPage builds a stable internal page — URL, human reason, a Try-again retry link, F5 hint — for DNS/refused/timeout and redirect-chain failures, in place of a blank document; the stub fallback still serves the demo pages, and only a genuine failure reaches the error page. URL is HTML-escaped. Demo: the M8 hub links offline.invalid. Tests: NetworkErrorPage + ResourceLoader unreachable-document tests.)*

---

## Milestone 8.5 Addendum — Modern-Layout Resilience (seznam.cz MVP)

*(Added 2026-07-23, after the seznam.cz investigation. Filed in `doc/TODOs.md`
under "Modern-portal CSS/layout gaps".)*

> **The Hacker News reply arrived, 2026-08-02: they now allow non-mainstream user
> agents.** HN was the reason browser identity and Compatibility mode were built
> at all, and the whole M8 North Star — login, comment, restart-safe session —
> now works under Hummingbird's **own** identity, with no escape hatch. That is
> the outcome asking for it was aiming at, and it is worth recording that the
> honest route was the one that eventually paid: the engine was never taught to
> impersonate Chrome by default, and did not have to be. Compatibility mode
> stays for the next server that behaves the way HN used to.

**Why this exists.** While the v0.8.0 release waits on the Hacker News team's reply,
seznam.cz was the first *modern flex+grid+absolute portal* pointed at the engine.
It is a full compatibility-ladder target (M10 positioning/scroll + M12 SPA) — this
addendum does **not** try to render it correctly. The bar is deliberately **"not
right, but somewhat navigable"**: stop the two most violent failures so a modern
portal degrades gracefully instead of detonating.

Two failure classes were diagnosed from the crawl (`tmp/seznam.cz`) + log:

* **(A) Giant icons** — replaced elements ignore percentage/`em`/`rem` sizing and
  `object-fit`, so the service-menu SVG icons render at their intrinsic ~200px
  instead of the ~40px their CSS asks for. → 8.5.1 + 8.5.2 + 8.5.4.
* **(B) Bleeding content** — no overflow clipping, so a child that overflows an
  `overflow:hidden` box paints across the rest of the page. → 8.5.3. *(The other
  contributor — absolute-positioning correctness — is out of scope here; it stays
  M10, tracked as `T-CSS-ABS-STATIC-1`. This slice reduces the overlap, it does not
  finish it, and that is the honest boundary.)*

**Non-goals (kept tight on purpose):** no scroll containers / `overflow:auto|scroll`
offsets / scrollbars (M10 "Overflow v2"); no flex/grid alignment completion
(`T-CSS-FLEX-ALIGNMENT-2`, `T-CSS-GRID-TEMPLATE-AREAS-1`, M10); no `aspect-ratio`,
multicol, `line-clamp`, or the cosmetic effect properties. This is four contained
rendering fixes, each independently demoable.

### 8.5.1: Percentage / `em` / `rem` Sizing For Replaced Elements (T-CSS-REPLACED-PERCENT-SIZE-1)

* **Goal:** size an `<img>`/SVG from a non-px CSS length instead of falling back to
  its intrinsic size.
* **Scope:** `ReplacedElementUtils::resolve_dimension`
  (`src/layout/geometry/metrics/ReplacedElementUtils.h`) resolves a styled dimension
  through `raw_px`, which reads only the **px** part of a `LengthValue` — so
  `width:100%` / `2rem` / `3em` on a replaced element resolves to ~0 and the code
  falls through to the intrinsic size. Resolve the percentage part against the
  containing block's content width/height in the replaced-sizing path. The original
  story assumed `em`/`rem` were already resolved to px at style-apply time; the
  2026-07-24 follow-up found that only partial `em` support existed and `rem` was
  absent, so that missed acceptance work is completed explicitly in 8.5.4.
* **Acceptance:** an `<img style="width:100%">` inside a 40px box lays out at 40px,
  not its intrinsic width; percentage `height` likewise resolves against the
  containing block.
* **Tests:** replaced-element sizing tests for `%` width/height (and a regression
  that `em`/`rem` still resolve). **Demo:** add to the M8 demo hub — an icon row
  sized in `%`/`rem` that renders small, not ballooned.

### 8.5.2: `object-fit` / `object-position` For Replaced Elements (T-CSS-OBJECT-FIT-1)

* **Goal:** honor `object-fit` (`contain`/`cover`/`fill`/`none`/`scale-down`) and
  `object-position` when painting a replaced element into a box whose size differs
  from the media's intrinsic ratio. *(Pulled forward from M10 — it pairs with 8.5.1:
  8.5.1 gives the box its size, `object-fit` fits the pixels inside it.)*
* **Scope:** parse the two properties into `ComputedStyle`; in the replaced-element
  paint path, compute the destination rect for the image inside its content box per
  the `object-fit` keyword and `object-position`. Default stays `fill` (today's
  stretch behavior) so nothing regresses.
* **Acceptance:** an `<img>` with a fixed box and `object-fit:contain` scales to fit
  inside the box preserving ratio (letterboxed), `cover` fills and crops, and
  `object-position` shifts the visible region.
* **Tests:** replaced-element paint/layout tests over the keywords. **Demo:** M8 hub
  — the same image in one fixed box under each `object-fit` value, side by side.

### 8.5.3: Paint-Time Clip For `overflow: hidden` / `clip` (T-CSS-OVERFLOW-CLIP-1)

* **Goal:** stop a child that overflows an `overflow:hidden` box from painting across
  the rest of the page.
* **Scope:** `ComputedStyle::overflow_x`/`overflow_y` already exist and the
  DisplayList already has `PushClip`/`PopClip` — block/flex boxes just never emit a
  clip. During paint, wrap a box whose computed overflow is `hidden`/`clip` in
  `push_clip(border-box)` … `pop_clip()`. **Clip only** — no scroll offset, no
  scrollbars, no hit-test change (those stay M10).
* **Acceptance:** a child larger than an `overflow:hidden` parent is visually clipped
  to the parent's box; an `overflow:visible` box is unaffected.
* **Tests:** display-list/paint clip tests (clip pushed for hidden, not for visible;
  nesting pops correctly). **Demo:** M8 hub — an oversized block inside a small
  `overflow:hidden` card, clipped to the card.

### 8.5.4: Complete `em` / `rem` Length Semantics (T-CSS-RELATIVE-LENGTH-1)

* **Goal:** make font-relative CSS lengths deterministic and distinct: `em` resolves
  against the element's computed font size, while `rem` resolves against the
  document root's computed font size.
* **Scope:** add a distinct `Unit::Rem` through parsing, serialization, custom-property
  substitution, and style application. Carry the root font-size reference through
  style computation rather than treating `rem` as `em` or hard-coding 16px. Ensure an
  element with no local `font-size` inherits its parent's size before resolving its
  own `em` lengths. On the root element itself, `rem` uses the initial 16px reference,
  avoiding a circular dependency when the root also sets `font-size`.
* **Acceptance:** Seznam's identical `.w-6.h-6` wrappers (`1.5rem` on each axis)
  compute to identical 24×24px boxes at the default root size regardless of the
  wrapped image's intrinsic dimensions. With a 20px root and a 10px local font,
  `2em` computes to 20px and `1.5rem` computes to 30px.
* **Tests:** parser test proving `em` and `rem` remain distinct; style tests for local,
  inherited, root-relative, and root-element resolution; replaced-layout regression
  proving identically authored icon boxes no longer follow intrinsic image sizes.
  **Demo:** the M8 layout page includes paired `em`/`rem` icon boxes.

### M8.5 Execution Checklist

- [x] 8.5.1: Percentage/`em`/`rem` sizing for replaced elements *(percent width/height now resolve against the containing block at the `layout(bounds)` seam, with the same definiteness guard as `BlockBox::resolve_height_constraint`; the inline-measure path keeps the bare-magnitude fallback. Tests: `ReplacedSizingTest.*` + `BlockBoxLayoutTest.ReplacedPercentWidth*`. Demo: example.dev/layout icon row.)*
- [x] 8.5.2: `object-fit` *(Fill/Contain/Cover/None/ScaleDown, centered; `cover`/`none` overflow is clipped to the content box via push_clip. New `ObjectFitUtils::compute_fit`; parsed+applied through the property registry. Tests: `ObjectFitTest.*` + `StyleEngineTest.AppliesObjectFitProperty`. Demo: example.dev/layout fit row. **`object-position` deferred** — `Value` is single-valued, so a 2-value position needs its own sub-struct/parser; centered default covers the seznam case. Filed as a follow-up on T-CSS-OBJECT-FIT-1.)*
- [x] 8.5.3: Paint-time clip for `overflow:hidden` *(RenderObject::paint wraps descendants in push_clip/pop_clip when BOTH axes are Hidden — the `overflow:hidden` shorthand; single-axis and scroll/auto deliberately not clipped, see the code comment. Tests: `OverflowClipTest.*`. Demo: example.dev/layout clip card.)*
- [x] 8.5.4: Complete `em`/`rem` semantics *(added and completed 2026-07-24 after the promised 8.5.1 regression exposed that `rem` was never represented and inherited-font `em` resolution was incomplete. `Unit::Rem` now stays distinct through parsing/serialization/var substitution and resolves through a root-font reference carried by style computation; inherited font size is available before box-length application. Tests: `CSSParserTest.KeepsEmAndRemAsDistinctLengthUnits`, `StyleEngineTest.ResolvesEmAndRemAgainstTheirCorrectFontSizes`, and `BlockBoxLayoutTest.ReplacedElementsHonorEmAndRemComputedSizes`. Demo: paired 24px `em`/`rem` icons on example.dev/layout.)*
- [x] Rebuild `Hummingbird.exe` (2026-07-23) — **user to re-check seznam.cz** (icons sane + content clipped). 791 tests green.

**Fixes from first in-app test (2026-07-23):** the demo surfaced three real bugs, all fixed:
1. **Percentage width ballooned to ~100000** when a replaced element was laid out during the intrinsic-measurement probe (the shrink-to-fit ballooning class of bug, reintroduced for replaced elements by 8.5.1). Fixed: `resolve_length` treats a percentage basis `>= kIntrinsicMeasureThreshold` (20000) as indefinite (bare magnitude), so a probe basis never resolves a percentage. Test `ReplacedSizingTest.PercentWidthDoesNotBalloonAgainstIntrinsicProbe`.
2. **`height:100%` icon rendered 44×100, not 44×44**, because `FlowLayout::layout_block_child` passes `bounds.height = 0` to block children, so a definite parent height never reaches a percentage-height child. Rather than the broad flow-layout change, added **intrinsic aspect-ratio preservation** (a replaced element with one axis specified and the other auto derives the auto one from the media ratio), and reworked the demo to lean on it. The real percentage-height propagation is filed as **T-CSS-PERCENT-HEIGHT-PROPAGATE-1** [M10]. Tests `ReplacedSizingTest.AspectRatio*`.
3. **overflow clip was the border box**; switched to the **padding box** (inside the border) so the border stays visible and nothing over-paints it.
Demo corrections: `.fitframe` was an inline `<span>` (ignores width/height) → made a sized box with an explicitly-sized image; icons now use `width:100%` + aspect ratio.

**External-review fix round (2026-07-23, 795 tests green).** Five findings triaged — two fixed, one hardened, two already-ticketed/filed:
1. **FIXED (the key finding): aspect ratio is now re-derived AFTER box-sizing/min-max.** The ratio ran before the clamps, so the ubiquitous `img { max-width:100% }` on a 1000×500 image in a 300px container produced a distorted 300×500. The unspecified axis now follows the clamped one (then re-applies its own min/max once); 300×150 as CSS2 §10.4 wants. Tests `MaxWidthClampFollowsAspectRatio`, `ClampedSpecifiedWidthRederivesAutoHeight`.
2. **FIXED: `overflow: clip` now parses**, mapping to Hidden — the two differ only in scroll affordances, which don't exist yet, so the milestone's "hidden/clip" wording is now true rather than narrowed. Test `OverflowClipParsesAsHidden`.
3. **HARDENED: the clip regression test now uses a 2px border**, so it actually distinguishes the padding-box clip from a border-box one (`ClipIsAtThePaddingBoxInsideTheBorder`).
4. **FILED: T-CSS-REPLACED-PERCENT-INLINE-1** — inline replaced elements have no containing block in `measure_inline`, so `%`/`max-width:100%` stays bare-magnitude there. Pre-existing (not an 8.5.1 regression); block/flex paths resolve.
5. **ACKNOWLEDGED: the ≥20000 probe threshold** misreading a genuinely huge container is the known cost of the sentinel-probe idiom — already ticketed as T-LAYOUT-INTRINSIC-SIZE-CONSTRAINT-1, which the reviewer independently endorsed as the clean fix.

**Pre-merge review (2026-07-24, 823 tests green).** The full milestone diff was
reviewed before merging `milestone/8` to `master`. One bug fixed directly:
**8.5.4's `rem` reference never reached `html { font-size: ... }`** — the engine
established it from the node handed to `StyleEngine::apply`, which in the real
pipeline is the HTML parser's synthetic `<root>` wrapper rather than `<html>`, so
every real page pinned `rem` to the initial 16px. The style unit tests passed
`<html>` in directly and so could not see it; the regression test now drives a
parsed tree. One pre-existing gap filed rather than fixed:
**T-CSS-SELECTOR-UNSUPPORTED-DROP-1** — an unsupported pseudo-class is dropped from
a compound selector instead of invalidating the rule, so `div:first-child` styles
every div and `a:not(.hidden)` styles exactly the excluded elements. Both are
written up under "M8 pre-merge review follow-ups" in `doc/TODOs.md`.
