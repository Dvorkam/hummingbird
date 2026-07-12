> **Status: Planned** — pre-written 2026-07, three milestones ahead but spec-driven
> (cookies/storage are RFC-shaped and nearly independent of M6/M7 discoveries).
> Sanity-check the network-seam story scopes against the codebase before kickoff.

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

---

## Critical Path (what must land for the North Star)

**Must-have**

* Cookie jar with matching/attribute policy (8.1.1, 8.1.2) wired into every engine
  request (document, subresource, POST).
* Redirect cookie semantics + chain hardening (8.1.3, 8.3.1).
* Jar persistence across restarts (8.1.4).
* `document.cookie` binding (8.1.5) — HN's login flow is form-POST, but session
  checks touch it.
* `localStorage` with persistence (8.2.1, 8.2.2).
* HN login/comment harness against a local fixture server (8.4.1).

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
- [ ] 8.1.1: Cookie Jar + Matching
- [ ] 8.1.2: Attribute Policy (Secure/HttpOnly/SameSite)
- [ ] 8.1.3: Redirect Cookie Semantics
- [ ] 8.3.1: Redirect Chain Hardening
- [ ] 8.1.4: Cookie Jar Persistence
- [ ] 8.1.5: document.cookie Binding

P0: Storage (North Star)
- [ ] 8.2.1: Storage Backing Store
- [ ] 8.2.2: localStorage Binding + Persistence

P0: Guardrails
- [ ] 8.4.1: Login-Flow Harness

P1: If Schedule Allows
- [ ] 8.2.3: sessionStorage (per-tab)
- [ ] 8.3.2: Network Error Pages
