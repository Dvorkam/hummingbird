> **Status: Active** — kickoff scope validation done 2026-07-26 (see below). Branch
> `milestone/9`, opened from `v0.8.0`.

## Kickoff Scope Validation (2026-07-26)

The pre-written draft asked to be revalidated against the codebase before
implementation. Five findings change the story list.

1. **Nine stories were still tagged `[M8]` when M8 shipped.** They were orphaned
   against a closed milestone. Five are genuine prerequisites for this milestone's
   own correctness rather than M8 leftovers, so they are folded in as **9.0
   Foundations** and run first; two more are carried as ordinary M9 backlog; two
   were re-homed to M11/M12. See "Carried Backlog" at the end.

2. **The proof target is retargeted.** The draft named "HNPWA browses live API
   data." Two problems: hnpwa.com's implementations date to 2017–2018 and are
   mostly framework-based (React/Vue/Angular/Preact/Polymer), which imports M12's
   dependencies; and "lists, threads, pagination" is client-side routing, which the
   roadmap assigns to **M12**. Verified live 2026-07-26: `api.hnpwa.com` and
   `hacker-news.firebaseio.com` both still serve, and Wikipedia's REST summary API
   serves CORS-friendly JSON. The proof is now anchored on those **APIs** rather
   than on a framework app — see the North Star below.

3. **An SPA-routing MVP is pulled forward from M12** (story 9.6.1), because a
   fetch-rendered page that cannot change its URL is not a browsing experience.
   Only the MVP: the full History API + SPA navigation work stays M12.

4. **Four gaps found in the M8 pre-merge review are now stories**: request
   deadlines (9.1.3), CORS across redirects (9.2.3), response-header exposure
   (9.2.4), and cache-key correctness (9.3.2). Each was missing from the draft
   entirely, and three of them are the halves of a feature that are easiest to skip
   and worst to skip.

5. **9.4's `browser.*` design was revalidated and changed shape.** The draft
   assumed a *synchronous* block/allow callback into extension JS. The current host
   (`doc/extensions.md`) runs one QuickJS context per extension, drives extensions
   through *events*, does not enforce the `permissions` field, and has no state
   persistence. A blocking call into extension JS on the request path would put
   script execution on the network hot path, with a hang risk and a re-entrancy
   hazard. 9.4.1 is now a **declarative rule** API (the MV3
   `declarativeNetRequest` shape): the extension registers patterns, the engine
   matches them natively.

---

## Milestone 9 North Star Deliverable

**Before (after Milestone 8):**

* Pages can hold a session (cookies/storage), but JS cannot make a network request.
  `fetch` is currently a *stub that returns a promise which never settles*
  (`QuickJSScriptEngine.cpp` prelude) — so an API-driven page does not fail, it
  hangs silently and forever.
* All resource loading is engine-initiated (document/CSS/image/font). There is no
  JS-visible request surface, no CORS model, and no HTTP caching: every navigation
  refetches everything.

**After (Milestone 9 done):**

* **fetch() v1** (Promise-based, headers, redirects, JSON round-trips, deadlines)
  and a minimal **XHR** compatibility wrapper.
* **CORS v1**, strict by default, enforced per redirect hop, relaxations only behind
  feature flags.
* **HTTP cache v1**: in-memory, `Cache-Control`/`ETag` revalidation, with a correct
  cache key (`Vary`) and correct handling of private/credentialed responses.
* **Declarative request filtering** in `browser.*` + a built-in **ad-block-lite**
  extension — the extension API's second real consumer.
* **Proof target: a page that renders from a live public API.** Two, deliberately:

  | Proof | What it proves | Verified |
  |---|---|---|
  | **Hacker News front page from `api.hnpwa.com/v0/news/1.json`** | The fetch → parse → DOM → render loop against real, hourly-changing third-party data, in one request per page | Endpoint live 2026-07-26 |
  | **Wikipedia REST summary (`/api/rest_v1/page/summary/<title>`)** | The **cross-origin** half: a formally documented, currently-maintained public API, fetched from a different origin, which is what actually exercises CORS | Endpoint live 2026-07-26, returns `title`/`extract`/`thumbnail` |

  Both are chosen because **M9's proof must be data-visible, not layout-visible**:
  every modern *site* will still render wrong until M10 lands positioning and scroll
  containers, but a list of text rows is what this engine already renders well. The
  demonstrable claim is "the browser now shows live data it fetched itself," not
  "the browser renders site X correctly."

### Why not a pinned HNPWA app build

The M7 precedent (pinned vanilla TodoMVC, "a fixed local fixture, not arbitrary
framework compatibility") is available and would be legitimate here: hnpwa.com does
list two non-framework entries ("Vanilla HN", webpack-bundled; "HNPWA Firebase",
pure HTML/CSS). Keep that as a **stretch** manual gate, not the North Star — a
2017-era third-party bundle failing for reasons unrelated to `fetch` would tell us
nothing about this milestone. Pin one only after 9.1.1 works against the API
directly.

---

## Non-Goals (keep the blast radius controlled)

* No streaming bodies (request or response) — buffered only; streaming is recorded
  as a follow-up for the media era.
* No Service Workers, no Cache API, no background sync.
* No cross-origin isolation features (COOP/COEP), no preflight cache tuning beyond
  a simple TTL.
* No on-disk HTTP cache — memory only, dropped at exit. Disk persistence is a
  follow-up (file it when the cache lands; it pairs with the profile-data work in
  `T-PROFILE-DATA-DIR-1`).
* Ad-block-lite uses a tiny curated filter list with substring/domain rules — **no
  EasyList syntax engine**, no cosmetic (CSS) filtering, no per-rule redirect or
  header rewriting.
* No full History API / SPA navigation: 9.6.1 is an explicit MVP and M12 owns the
  rest.
* No fetch niceties beyond the target's needs: no `AbortController` unless a proof
  target forces it (telemetry will say), no `FormData` bodies beyond
  urlencoded/JSON, no `Request`/`Response` cloning semantics.
* No extension security boundary. 9.4 adds permission *gating* for the new API, not
  a sandbox; `doc/extensions.md`'s standing warning still applies.

---

## Critical Path (what must land for the North Star)

**Must-have**

* 9.0.1 microtask ordering and 9.0.2 per-document global isolation — both are
  load-bearing the moment promises and in-flight requests exist.
* fetch() with a Request/Response/Headers minimum surface (9.1.1) riding the M8
  cookie jar and M7 microtask pump, with deadlines (9.1.3).
* Same-origin + CORS classification, preflight, strict enforcement (9.2.1),
  re-checked per redirect hop (9.2.3).
* In-memory HTTP cache with revalidation (9.3.1) and a correct cache key (9.3.2) —
  the proof targets hammer the same endpoints.
* The API-render harness (9.5.1) against a mock API server.

**Nice-to-have (if schedule allows)**

* XHR wrapper (9.1.2) — only if a proof target actually uses it. The existing
  missing-API telemetry stub already reports `XMLHttpRequest` when a page touches
  it, so this is a data-driven decision, not a guess.
* Declarative request filtering + ad-block-lite (9.4.1, 9.4.2).
* Preflight result caching (9.2.2).
* SPA routing MVP (9.6.1) — required for "browse a thread and come back," optional
  for "render the front page."

---

## Milestone 9 Done When

* A page renders the **live** Hacker News front page from `api.hnpwa.com`, and a
  page renders a **cross-origin** Wikipedia REST summary; the same flows pass in CI
  against a mock API server.
* CORS matrix tests pass: simple vs preflighted, allowed vs blocked, credentials
  behavior with the M8 cookie jar, and **a cross-origin redirect mid-chain**.
* Cache tests pass: fresh hit (no request), stale revalidation (304 path),
  `no-store` honored, and two requests differing only in a `Vary`-named header do
  not share an entry.
* No fetch can hang indefinitely: every request has a deadline and a surfaced
  failure.
* With ad-block-lite enabled, filtered requests never reach the network, the page
  still renders, and the run reports **requests and bytes avoided** (if 9.4 lands).
* Missing-API telemetry from the proof-target runs is triaged into the M12 backlog.

---

## Stories

### 9.0 - Foundations (prerequisites carried from the M8 backlog)

These are not new work items; they are existing tickets that turn out to gate this
milestone. They run **first** because 9.1–9.3 build directly on them.

* **Story 9.0.1: Microtask Drain Depth Guard (T-DISPATCH-MICROTASK-REENTRANT-1)**
* **Goal:** drain the promise microtask queue only at the outermost script
  dispatch, not on every re-entrant nested dispatch.
* **Scope:** `QuickJSScriptEngine::dispatch_dom_event` calls `drain_microtasks()`
  unconditionally. Story 7.7.1 added a dispatch-depth-gated mutation epoch for
  exactly this re-entrancy pattern; the microtask drain has no equivalent guard, so
  a nested dispatch runs queued promise continuations mid-script, out of the order
  a real event loop would.
* **Why it is foundational now:** M9 is the milestone that makes the microtask
  queue load-bearing. Every `fetch().then()` resolves through it. A latent ordering
  bug in M8 becomes an observable "my callback ran at the wrong time" bug the moment
  fetch exists, and it will be diagnosed as a fetch bug.
* **Acceptance:** a click handler that queues a `Promise.then` and synchronously
  calls `el.focus()` does not run the `.then` callback until the outer click
  dispatch completes.
* **Tests:** script-engine reentrancy tests.

* **Story 9.0.2: Per-Document JS Global Isolation (T-JS-GLOBAL-ISOLATION-1)**
* **Goal:** give each navigated document a fresh JS global, so one page's script
  globals do not leak into the next page in the same tab.
* **Scope:** `IScriptEngine::reset_bindings()` clears per-document references
  (listeners, timers, node wrappers) but the `JSContext` — and therefore the global
  object — persists for the tab's lifetime. Recreate the context (or clear
  script-added globals) on navigation and reinstall the binding surface; per-runtime
  class IDs persist, but retained per-context values (`document_object_`,
  `window_object_`) must be rebuilt.
* **Why it is foundational now:** 9.1.1's acceptance already requires that
  "in-flight fetches are cancelled on document teardown without callbacks firing
  into a dead page." That is the same teardown problem seen from the other end. A
  pending fetch continuation captured by a stale global is exactly the case this
  ticket describes, and 9.1.1 cannot be honestly closed without it.
* **Acceptance:** a global set on page A is `undefined` on page B in the same tab;
  the teardown suite's `NavigationTeardownReleasesPerDocumentState` flips its final
  assertion from `=== 'page-a'` to `=== undefined`.
* **Tests:** engine teardown tests.

* **Story 9.0.3: Cookie Hardening For Script-Driven Requests**
* **Goal:** close the three cookie gaps that stop being theoretical once a page can
  issue its own requests. Split into three sub-stories so each lands as its own
  reviewable commit; they are independent and can be read in any order, but the
  numbering is the intended sequence.

* **Story 9.0.3.1: Registrable Domain From A Public Suffix List
  (T-COOKIE-PUBLIC-SUFFIX-1, P1)**
* **Goal:** stop approximating the registrable domain as "the last two labels."
* **Scope:** `is_same_site` (`core/net/Cookie.cpp`) takes the last two labels as
  the registrable domain, and `parse_set_cookie` will therefore accept
  `Domain=co.uk`. CORS credentials decisions lean on that same function, so the
  approximation graduates from "cookie scoping wart" to "cross-origin policy
  input." **Decided at kickoff: a curated multi-label suffix list, not the full
  ICANN PSL** — a hand-maintained table of the suffixes that actually matter
  (`co.uk`, `com.au`, `github.io`, …) plus the wildcard/exception rules the format
  needs. The full list is a vendored data file with its own refresh, versioning,
  and staleness policy; that is a separate ticket, filed when this lands.
* **Acceptance:** `Domain=co.uk` from `example.co.uk` is rejected while
  `Domain=example.co.uk` is accepted; `a.co.uk` and `b.co.uk` are not same-site;
  `a.example.com` and `b.example.com` still are.
* **Tests:** parse + same-site matrix over `com`/`co.uk`/`github.io`.

* **Story 9.0.3.2: Cookie Jar Limits (T-COOKIE-LIMITS-1)**
* **Goal:** bound what a page can put in the jar.
* **Scope:** the jar enforces no size or count limits at all. Apply the RFC 6265
  §6.1 minimums: 4096-byte name+value, 50 per domain, 3000 total, LRU eviction
  within a domain. fetch hands a page unbounded request volume, and therefore
  unbounded `Set-Cookie` volume, which makes this a page-controlled
  resource-exhaustion path into a file the browser writes to disk.
* **Acceptance:** an oversized cookie is refused; the per-domain and total caps
  hold; eviction within a domain is least-recently-used.
* **Tests:** jar limit tests.

* **Story 9.0.3.3: Cookie Charset Validation (T-COOKIE-CHARSET-1)**
* **Goal:** reject cookies whose bytes would not survive a round trip.
* **Scope:** RFC 6265 §4.1.1 token/cookie-octet validation in `parse_set_cookie` —
  reject CTLs and the separators that would let a stored value forge a second
  cookie when re-serialized. fetch also lets JS set request headers, so this is the
  header-injection-shaped half.
* **Acceptance:** a `Set-Cookie` carrying CTLs or an embedded `;` is rejected
  rather than round-tripped.
* **Tests:** parse-level charset matrix.

*(T-COOKIE-CONFORMANCE-VECTORS-1 stays in the carried backlog. Its precondition
"settle redirect behavior first" is now met and it would give this slice a tracked
number, but it is a separate pass over conformance vectors, not part of these
three fixes.)*

### 9.1 - fetch/XHR v1

* **Story 9.1.1: fetch() Core**
* **Goal:** `fetch(url, {method, headers, body})` returning a Promise of a Response
  with `status`, `headers`, `text()`, `json()`.
* **Scope:** JS binding → engine request path (the M8 cookie jar, redirect chain,
  and per-origin identity all apply); buffered bodies; per-document cancellation on
  navigation. The existing `fetch` stub in the QuickJS prelude must be **removed**,
  not shadowed — it is guarded by `typeof === 'undefined'`, so a real binding wins,
  but leaving a never-settling promise in the tree is a trap.
* **Acceptance:** GET and POST (JSON/urlencoded) round-trip; in-flight fetches are
  cancelled on document teardown without callbacks firing into a dead page.
* **Tests:** binding + engine integration tests (mock server).

* **Story 9.1.2: XHR Compatibility Wrapper**
* **Goal:** minimal `XMLHttpRequest` (open/send/onreadystatechange/responseText/
  status) implemented over the fetch path.
* **Scope:** JS-side wrapper; no separate native path. Replaces the current
  reporting stub.
* **Acceptance:** an XHR-based page works.
* **Tests:** wrapper tests.
* **Pull-in trigger:** the missing-API telemetry already reports `XMLHttpRequest`
  on any page that touches it. Build this when a proof target reports it, not
  speculatively.

* **Story 9.1.3: Request Deadlines (gap found in the M8 pre-merge review)**
* **Goal:** no request can hang forever, and a request that gives up produces a
  real, surfaced failure.
* **Scope:** nothing in the engine currently bounds a request's lifetime — M8
  shipped error *pages* for navigation, but a stalled connection has no deadline.
  Today's `fetch` stub is literally `new Promise(function () {})`, so the *current*
  behavior is an eternal hang; shipping real fetch without a deadline replaces one
  silent hang with another. Add a connect/total deadline at the engine request seam
  (so document, subresource, and fetch traffic all inherit it), map it to a
  `NetworkError` variant, reject the fetch promise with a distinguishable error,
  and thread it through to the M8 network-error page for navigations.
* **Acceptance:** a request to a black-holed endpoint rejects within the deadline
  with a timeout-shaped error rather than hanging; a navigation to the same
  endpoint renders the network-error page; the deadline is configurable for tests.
* **Tests:** engine timeout tests against a deliberately stalling fixture; a fetch
  binding test asserting the rejection reason.

### 9.2 - CORS v1 (strict)

* **Story 9.2.1: Request Classification + Enforcement**
* **Goal:** same-origin vs cross-origin classification; simple vs preflighted
  requests; `Access-Control-Allow-*` response checks; credentials mode interacting
  correctly with the cookie jar.
* **Scope:** engine request pipeline; strict by default, per-flag relaxation only.
  Reuse `Core::Origin` (M8) as the origin comparison primitive rather than
  introducing a second notion of origin.
* **Acceptance:** a disallowed cross-origin fetch rejects **without exposing the
  response** (status, headers, or body); an allowed one resolves. Credentialed
  requests to an origin answering `Access-Control-Allow-Origin: *` are rejected per
  spec.
* **Tests:** CORS matrix tests (mock server).

* **Story 9.2.2: Preflight Cache (simple TTL)**
* **Goal:** avoid re-preflighting every request to the same endpoint.
* **Scope:** small keyed cache honoring `Access-Control-Max-Age`.
* **Acceptance:** repeated preflighted requests preflight once within the TTL.
* **Tests:** cache behavior tests.

* **Story 9.2.3: CORS Across Redirect Hops (gap found in the M8 pre-merge review)**
* **Goal:** apply the CORS check to every hop of a redirect chain, not just the
  first request.
* **Scope:** M8 took ownership of the redirect loop from libcurl and already
  recomputes the `Cookie` header, the SameSite context, the `Referer`, and the
  per-origin identity **per hop** at the `send_request` choke point. The CORS check
  must join that list: a cross-origin hop re-runs classification against the new
  target, a chain that leaves the original origin taints the request (subsequent
  hops are cross-origin even if they return to the original host), and a redirect
  that a preflighted request is not allowed to follow fails rather than silently
  following. `RedirectChain` already carries the per-hop context this needs.
* **Why it is separate:** this is the half of CORS that is invisible in a
  same-origin test suite and is the classic way a strict CORS implementation ends
  up not being strict. It is cheap here precisely because M8 built the seam.
* **Acceptance:** a cross-origin fetch that 302s to a third origin is evaluated
  against that third origin; a chain returning to the initiator's origin is still
  treated as cross-origin; a disallowed hop rejects without exposing the response.
* **Tests:** extend the CORS matrix with redirect cases, alongside the existing
  `RedirectChain` suite.

* **Story 9.2.4: Response Header Exposure (gap found in the M8 pre-merge review)**
* **Goal:** let JS read only the response headers CORS permits.
* **Scope:** 9.2.1 checks what the *server* allows for the request; this is the
  other direction — which response headers the *page* may observe. Implement the
  CORS-safelisted response header set (`Cache-Control`, `Content-Language`,
  `Content-Type`, `Expires`, `Last-Modified`, `Pragma`) plus whatever
  `Access-Control-Expose-Headers` names, and filter `Response.headers` for
  cross-origin responses. Same-origin responses expose everything.
* **Acceptance:** a cross-origin `Response.headers` iteration yields only safelisted
  headers; adding `Access-Control-Expose-Headers: X-Total-Count` makes exactly that
  header readable; `Set-Cookie` is never readable.
* **Tests:** header-exposure tests in the CORS matrix.

### 9.3 - HTTP Cache v1 (memory first)

* **Story 9.3.1: Cache Core + Revalidation**
* **Goal:** in-memory cache with a `Cache-Control` baseline (`max-age`,
  `no-store`, `no-cache`) and `ETag`/`If-None-Match` → 304 revalidation.
* **Scope:** sits at the engine network seam so document, subresource, and fetch
  traffic all benefit; bounded size with LRU eviction. Note the seam moved in M8 —
  `ResourceLoader::send_request` is now the choke point where cookies, identity, and
  referrer are applied per hop; the cache belongs there too, and must sit on the
  correct side of the redirect loop (cache the individual hop responses, not the
  chain).
* **Acceptance:** a reload of a cached page issues conditional requests and reuses
  bodies on 304; `no-store` is never cached.
* **Tests:** cache unit + integration tests.

* **Story 9.3.2: Cache Key Correctness — `Vary`, `private`, Credentials (gap found
  in the M8 pre-merge review)**
* **Goal:** never serve a cached response to a request it was not negotiated for.
* **Scope:** a cache keyed only on URL+method is wrong the moment responses vary,
  and M8 made that concrete: the `User-Agent` **now differs per origin** (per-origin
  compatibility mode), and `T-NET-CLIENT-HINTS-1` will add `Accept-CH`-driven
  request headers on top. Honor `Vary` by including the named request headers in the
  cache key (and treat `Vary: *` as uncacheable); honor `Cache-Control: private` by
  refusing to store; and decide-and-document the rule for **credentialed**
  responses (a response fetched with cookies must not be served to a request
  without them).
* **Why it is separate:** this is cache *correctness*, not cache *features*. A
  `Vary`-blind cache silently serves the wrong variant, which presents as an
  unreproducible rendering bug rather than as a cache bug.
* **Acceptance:** two requests differing only in a `Vary`-named header do not share
  an entry; flipping a site's identity mode does not serve it the other mode's
  cached response; `Cache-Control: private` is never stored; `Vary: *` is never
  cached.
* **Tests:** cache-key tests per header, plus an identity-mode round trip.

### 9.4 - Extension Follow-Through (ad-block-lite)

*Reshaped at kickoff. The deliverable here is the **hook**; the extension is its
proof. See finding 5 above for why the synchronous-callback design was dropped.*

* **Story 9.4.1: Declarative Request-Filtering Rules**
* **Goal:** let an extension declare, up front, which requests the engine should
  block — with no extension JS on the request path.
* **Scope:** a `browser.declarativeRequest`-lite API (name TBD; the MV3
  `declarativeNetRequest` shape is the model): an extension registers a rule set
  (domain and substring patterns, optionally scoped by resource type and initiator)
  from its background script at startup, and the engine matches those rules
  natively at the `ResourceLoader::send_request` choke point. Blocked requests
  resolve to an explicit `Blocked` resource state — never a hang, and
  distinguishable from a network failure so the M8 error-page path is not triggered.
  Deterministic rule ordering; disabled extensions are never consulted (M5
  lifecycle rules apply). **Two host gaps this story must close as a side effect:**
  the `permissions` manifest field is currently parsed but not enforced, and this is
  the first API that must be permission-gated; and rule sets need to survive a
  restart, which the host has no persistence for today.
* **Why declarative, not a callback:** a synchronous verdict from extension JS puts
  a QuickJS call — and therefore arbitrary script, including a possible re-entrant
  one — inside the network path for every request. Declarative rules are matched in
  C++, are trivially unit-testable without a script engine, cannot hang, and are the
  direction shipping browsers took for exactly these reasons.
* **Acceptance:** a background script registers block patterns and matching
  requests never reach the network; the resource state reads `Blocked`; an extension
  without the required permission cannot register rules; rules survive a restart.
* **Tests:** rule-matcher unit tests (no script engine) + extension API integration
  tests + a permission-denial test.

* **Story 9.4.2: Built-In Ad-Block-Lite Extension**
* **Goal:** ship the second canonical bundled extension — a small curated filter
  list, toggleable like Dark Mode — as the working proof of 9.4.1.
* **Scope:** background script + list format + docs, under
  `assets/extensions/ad-block-lite/`. Curated domain/substring rules only; no
  EasyList parsing, no cosmetic filtering.
* **Acceptance — stated as measurement, not as pixels.** An ad-blocker cannot be
  demonstrated visually by this engine yet: a page whose layout is already wrong
  does not visibly improve when boxes are removed from it. The honest claims are
  countable, so the acceptance criteria are countable:
  1. **Requests and bytes avoided** — a fixture page with tracker-shaped requests
     loads with them blocked, and the run reports how many requests and how many
     bytes were not fetched.
  2. **Node count and layout time** — blocking third-party markup reduces DOM node
     count, which reduces layout time. The HN item page already measures
     `layout ms ≈ 1900` at ~32k nodes (`T-PERF-LAYOUT-INCREMENTAL-1`), so this is
     the first change that can make a real page measurably *usable* rather than
     merely lighter. Report both numbers before/after on a real ad-bearing page.
  3. **Log noise avoided** — third-party ad/analytics JS is precisely the code that
     hits unimplemented APIs. Report the drop in missing-API telemetry and
     compatibility-warning volume (the M8 `CompatibilityWarnings` summary already
     counts this).
  4. **Rendering is not worse** — the first-party page still renders. Removing
     third-party JS can make a page render *better*, by removing script errors that
     abort the first-party page's own initialization; if that happens, say so with
     a before/after.
* **Tests:** integration test over the fixture page (counts, not screenshots) plus
  a manual check on a real ad-bearing page.
* **Candidate manual target:** a Czech news portal such as novinky.cz — there is
  already a crawl and a diagnosis of it from the M8.5 seznam.cz investigation, so
  the before/after has a documented baseline.

### 9.5 - Guardrails

* **Story 9.5.1: API-Render Harness (T-API-RENDER-E2E-1, formerly T-HNPWA-E2E-1)**
* **Goal:** a CI-runnable end-to-end proof of the fetch → render loop, driven
  headlessly against a mock API server, plus a live manual gate.
* **Scope:** fixture pages + harness in the pattern of the M6–M8 harnesses
  (`T-DDG-E2E-1`, `T-TODOMVC-E2E-1`, `LoginFlow.test.cpp`). Two flows:
  1. **Same-origin list render** — fetch an HNPWA-shaped story-list JSON payload,
     build DOM from it, assert the rendered rows (titles, points, comment counts).
  2. **Cross-origin summary render** — fetch a Wikipedia-summary-shaped payload
     from a *different* origin with CORS headers, assert both the allowed render and
     the blocked case when the mock withholds `Access-Control-Allow-Origin`.
  The live run against `api.hnpwa.com` and `en.wikipedia.org` stays a manual gate,
  as with live HN in M8.
* **Acceptance:** CI fails when the fetch/render loop, the CORS decision, or the
  cache path regresses.
* **Tests:** the harness is the test.
* **Kickoff check (5 minutes, needs network):** confirm both live endpoints' CORS
  response headers with `curl -I` before committing to the cross-origin manual
  gate. Both endpoints were confirmed to *serve* on 2026-07-26; their
  `Access-Control-Allow-Origin` headers were **not** directly inspected, because the
  dev sandbox has no network. If Wikipedia's REST API turns out not to be
  CORS-open, the mock-server flow still proves 9.2.x and only the manual gate
  changes.

### 9.6 - SPA Routing MVP (pulled forward from M12)

* **Story 9.6.1: History API MVP**
* **Goal:** let a fetch-rendered page change the URL and respond to back/forward
  without a document teardown — the minimum that makes API-driven browsing feel
  like browsing.
* **Scope:** `history.pushState`/`replaceState` and the `popstate` event, wired to
  the existing tab history. Nothing of this exists today (verified: no `pushState`,
  `replaceState`, or `popstate` anywhere in `src/`). `hashchange` **does** work
  (story 7.2.5), so the cheapest correct staging is:
  - **Stage A (in scope):** hash-based routing is already viable — confirm it works
    for a list → detail → back flow and cover it in the harness.
  - **Stage B (in scope, MVP):** `pushState`/`replaceState` update the URL bar and
    the tab's history entry; `popstate` fires on back/forward with the stored state
    object; no document teardown occurs.
  - **Out of scope (stays M12):** scroll restoration, navigation interception,
    same-document navigation edge cases, `history.state` serialization beyond
    structured-clone-of-JSON, and interaction with M10's scroll containers.
* **Why pull it forward:** the roadmap assigns History API + SPA navigation to M12,
  but M9's proof target is a page that renders lists and details from an API. The
  full M12 story is not needed; the URL-changing MVP is. Splitting it here keeps
  M9's proof honest without importing M12's framework surface.
* **Dependency:** builds on 9.0.2 (per-document global isolation) — a same-document
  route change must *not* reset the global, which is only a meaningful distinction
  once teardown semantics are correct.
* **Acceptance:** a page fetches a list, `pushState`s a detail route, renders the
  detail from a second fetch, and browser Back returns to the list view with
  `popstate` fired and no document reload.
* **Tests:** script-binding tests for the three APIs + a harness flow asserting no
  document rebuild across a route change.

---

## Carried Backlog (tracked in `doc/TODOs.md`, not restated here)

Ordinary M9 work items that are not part of the North Star path. They live in
`doc/TODOs.md` with full rationale; this is the index.

| Ticket | P | Note |
|---|---|---|
| `T-NET-IDENTITY-UI-1` | P1 | **Promoted from P2.** Since `c18dc9a` removed the `Ctrl+Shift+U` instructions from the README, the only route to a *shipped* feature is an undocumented keyboard shortcut. A shipped feature reachable only by a secret is a hole, not a nicety. |
| `T-COOKIE-CONFORMANCE-VECTORS-1` | P2 | Its stated precondition ("do this after 8.3.1/8.1.3 so redirect behavior is settled") is now met. Cheap, and it gives 9.0.3 a tracked number. |
| `T-FORM-PASSWORD-MASK-1` | P1 | Carried from M8's HN login. Privacy gap on a control people type secrets into. |
| `T-FORM-INPUT-PASTE-1` | P1 | Carried from M8's HN login. Precursor to M11's clipboard work. |
| `T-STORAGE-DOT-ACCESS-1` | P2 | `localStorage.foo` dot/bracket access, deferred from 8.2.2. |
| `T-NET-IDENTITY-AUTOOFFER-1` | P2 | Phase 2 of the identity work; depends on `T-NET-IDENTITY-UI-1` for its surface. |
| `T-HTML-PRESENTATIONAL-TAGS-1` | P2 | `<center>`/`<u>`; small and contained. |
| `T-FONT-WOFF2-1` | P2 | WOFF2 decoding. Independent of everything else here. |
| `T-NET-CLIENT-HINTS-1` | P3 | `Accept-CH`. **Note the coupling:** it adds request headers that responses may `Vary` on, so land it *after* 9.3.2 or the cache key will be wrong for exactly the headers this adds. |

**Re-homed at kickoff** (were tagged `[M8]`, do not belong here):
`T-FOCUS-MUTATION-SYNC-1` → M11 (focus system), `T-EVENT-SPEC-GAPS-1` → M12 (event
constructors and listener options are framework-surface items).

---

## Execution Order Checklist

P0: Foundations (must precede fetch)
- [x] 9.0.1: Microtask Drain Depth Guard *(`QuickJSScriptEngine` now tracks a
      `script_entry_depth_`; every JS entry point — eval, DOM event dispatch, the
      hashchange dispatch, and each timer/rAF callback — brackets its JS execution
      with a `ScriptEntryScope`, and `drain_microtasks()` returns early while the
      depth is non-zero. The outermost entry drains what the whole re-entrant chain
      queued. Test: `ScriptEngineTest.NestedDispatchDefersMicrotaskCheckpointToOutermost`
      — a click listener queues a `.then`, calls `focus()` (which re-enters through
      the host as a nested focus dispatch), and the continuation runs only after the
      click listener returns. Verified failing without the guard.)*
- [x] 9.0.2: Per-Document JS Global Isolation *(`reset_bindings()` now frees the
      `JSContext` and builds a new one, so each navigated document gets a fresh JS
      global. Class IDs are per-runtime and are registered once in the constructor
      (`register_runtime_classes`); everything per-context — the Node and
      DOMTokenList prototypes, console/document/window/extension bindings, the
      fail-soft stubs, `document_object_`/`window_object_` — is rebuilt by
      `create_context()`, which reinstalls whatever the currently bound hosts call
      for. Teardown of per-document JS references split into
      `release_document_state()` so the destructor does not build a context it is
      about to free. Tests: `NavigationGivesTheNextDocumentAFreshGlobal` (a global,
      a function declaration, and a clobbered `document.getElementById` all fail to
      reach page B), plus the rewritten `NavigationTeardownReleasesPerDocumentState`
      and `ResetBindingsRebuildsWrappersForTheNextDocument`. Two older tests
      observed teardown *through* a surviving global and were repointed at the DOM,
      which is the only channel that spans both documents. Demo:
      example.dev/m9 ↔ example.dev/isolation-next.)*
- [ ] 9.0.3.1: Registrable Domain From A Public Suffix List
- [ ] 9.0.3.2: Cookie Jar Limits
- [ ] 9.0.3.3: Cookie Charset Validation

P0: fetch + CORS (North Star)
- [ ] 9.1.1: fetch() Core
- [ ] 9.1.3: Request Deadlines
- [ ] 9.2.1: Request Classification + Enforcement
- [ ] 9.2.3: CORS Across Redirect Hops
- [ ] 9.2.4: Response Header Exposure

P0: Cache (North Star)
- [ ] 9.3.1: Cache Core + Revalidation
- [ ] 9.3.2: Cache Key Correctness — `Vary`, `private`, Credentials

P0: Guardrails
- [ ] 9.5.1: API-Render Harness

P1: If Schedule Allows
- [ ] 9.6.1: History API MVP *(needed for "browse a thread and come back")*
- [ ] 9.4.1: Declarative Request-Filtering Rules
- [ ] 9.4.2: Built-In Ad-Block-Lite Extension
- [ ] 9.1.2: XHR Compatibility Wrapper *(only if telemetry reports it)*
- [ ] 9.2.2: Preflight Cache
