> **Status: Next — revalidate at kickoff.** Pre-written 2026-07. M7's microtask pump
> and M8's session/network groundwork now exist; confirm their current seams before
> implementation. The `browser.*` request-hook design in 9.4 still assumes the M5-era
> extension-host shape. Rewrite story Scope lines against the current codebase before
> starting.

## Milestone 9 North Star Deliverable

**Before (after Milestone 8):**

* Pages can hold a session (cookies/storage), but JS cannot make a network request —
  every SPA that renders from an API is a blank shell.
* All resource loading is engine-initiated (document/CSS/images); there is no
  JS-visible request surface, no CORS model, and no HTTP caching.

**After (Milestone 9 done):**

* **fetch() v1** (Promise-based, headers, redirects, JSON round-trips) and a minimal
  **XHR** compatibility wrapper.
* **CORS v1**, strict-by-default, relaxations only behind feature flags.
* **HTTP cache v1**: in-memory, Cache-Control/ETag revalidation.
* **`browser.*` request-filtering hook** + built-in **ad-block-lite** extension — the
  extension API's second real consumer, making every later target page lighter.
* **Proof target:** **HNPWA (Hacker News PWA)** browses live API data — story lists,
  threads, pagination — rendered entirely from fetch responses.

---

## Non-Goals (keep the blast radius controlled)

* No streaming bodies (request or response) — buffered only; streaming is recorded as
  a follow-up for the media era.
* No Service Workers, no Cache API, no background sync.
* No cross-origin isolation features (COOP/COEP), no preflight cache tuning beyond a
  simple TTL.
* Ad-block-lite uses a tiny curated filter list with substring/domain rules — no
  EasyList syntax engine.
* No fetch niceties beyond the target's needs: no AbortController (unless HNPWA
  forces it — telemetry will say), no FormData bodies beyond urlencoded/JSON.

---

## Critical Path (what must land for the North Star)

**Must-have**

* fetch() with Request/Response/Headers minimum surface (9.1.1) riding the M8 cookie
  jar and M7 microtask pump.
* Same-origin + CORS request classification, preflight, strict enforcement (9.2.1).
* In-memory HTTP cache with revalidation (9.3.1) — HNPWA hammers the same endpoints.
* HNPWA harness against a mock API server (9.5.1).

**Nice-to-have (if schedule allows)**

* XHR wrapper (9.1.2) — only if the chosen HNPWA build (or telemetry on other
  targets) actually uses it.
* Request-filtering hook + ad-block-lite (9.4.1, 9.4.2).
* Preflight result caching (9.2.2).

---

## Milestone 9 Done When

* HNPWA (pinned build) renders lists/threads/pagination from live API calls, and the
  same flow passes in CI against a mock API server.
* CORS matrix tests pass: simple vs preflighted, allowed vs blocked, credentials
  behavior with the M8 cookie jar.
* Cache tests pass: fresh hit (no request), stale revalidation (304 path), no-store
  honored.
* With ad-block-lite enabled, filtered requests never hit the network and pages still
  render (if 9.4 lands).
* Missing-API telemetry from the HNPWA run is triaged into the M12 backlog.

---

## Stories

### 9.1 - fetch/XHR v1

* **Story 9.1.1: fetch() Core**
* **Goal:** `fetch(url, {method, headers, body})` returning a Promise of a Response
  with `status`, `headers`, `text()`, `json()`.
* **Scope:** JS binding → engine request path (cookie jar, redirects from M8 apply);
  buffered bodies; per-document cancellation on navigation.
* **Acceptance:** GET and POST(JSON/urlencoded) round-trip; in-flight fetches are
  cancelled on document teardown without callbacks firing into a dead page.
* **Tests:** binding + engine integration tests (mock server).

* **Story 9.1.2: XHR Compatibility Wrapper**
* **Goal:** minimal XMLHttpRequest (open/send/onreadystatechange/responseText/status)
  implemented over the fetch path.
* **Scope:** JS-side wrapper; no separate native path.
* **Acceptance:** an XHR-based page (or the HNPWA variant that uses it) works.
* **Tests:** wrapper tests.

### 9.2 - CORS v1 (strict)

* **Story 9.2.1: Request Classification + Enforcement**
* **Goal:** same-origin vs cross-origin classification; simple vs preflighted
  requests; `Access-Control-Allow-*` response checks; credentials mode interacting
  correctly with the cookie jar.
* **Scope:** engine request pipeline; strict by default, per-flag relaxation only.
* **Acceptance:** disallowed cross-origin fetch rejects without exposing the
  response; allowed one resolves.
* **Tests:** CORS matrix tests (mock server).

* **Story 9.2.2: Preflight Cache (simple TTL)**
* **Goal:** avoid re-preflighting every request to the same endpoint.
* **Scope:** small keyed cache honoring `Access-Control-Max-Age`.
* **Acceptance:** repeated preflighted requests preflight once within TTL.
* **Tests:** cache behavior tests.

### 9.3 - HTTP Cache v1 (memory first)

* **Story 9.3.1: Cache Core + Revalidation**
* **Goal:** in-memory cache keyed by URL+method with Cache-Control baseline
  (max-age, no-store, no-cache) and ETag/If-None-Match → 304 revalidation.
* **Scope:** sits at the engine network seam so document, subresource, and fetch
  traffic all benefit; bounded size with LRU eviction.
* **Acceptance:** reload of a cached page issues conditional requests and reuses
  bodies on 304; no-store is never cached.
* **Tests:** cache unit + integration tests.

### 9.4 - Extension Follow-Through (ad-block-lite)

* **Story 9.4.1: Request-Filtering Hook**
* **Goal:** `browser.webRequest`-lite: extensions register a synchronous
  block/allow decision on request metadata (URL, type, initiator) before dispatch.
* **Scope:** extension host API + engine request pipeline hook; deterministic
  ordering; disabled extensions never consulted (M5 lifecycle rules apply).
* **Acceptance:** a background script can block requests by pattern; blocking is
  observable in resource state (Failed/Blocked, not hung).
* **Tests:** extension API integration tests.

* **Story 9.4.2: Built-In Ad-Block-Lite Extension**
* **Goal:** ship the second canonical extension: small curated filter list
  (domains + substrings), toggleable like Dark Mode.
* **Scope:** background script + list format + docs.
* **Acceptance:** a fixture page with tracker-shaped requests loads with them
  blocked and renders correctly.
* **Tests:** integration test + manual check on real pages.

### 9.5 - Guardrails

* **Story 9.5.1: HNPWA Harness (T-HNPWA-E2E-1)**
* **Goal:** pinned HNPWA build + mock API server driven headlessly: load shell,
  fetch list, open thread, paginate.
* **Scope:** fixture + harness (pattern of the M6–M8 harnesses); live-site run stays
  a manual gate.
* **Acceptance:** CI fails when the fetch/render loop regresses.
* **Tests:** integration harness.

---

## Execution Order Checklist

P0: fetch + CORS (North Star)
- [ ] 9.1.1: fetch() Core
- [ ] 9.2.1: Request Classification + Enforcement

P0: Cache (North Star)
- [ ] 9.3.1: Cache Core + Revalidation

P0: Guardrails
- [ ] 9.5.1: HNPWA Harness

P1: Pull In As Needed
- [ ] 9.1.2: XHR Compatibility Wrapper
- [ ] 9.2.2: Preflight Cache
- [ ] 9.4.1: Request-Filtering Hook
- [ ] 9.4.2: Built-In Ad-Block-Lite Extension
