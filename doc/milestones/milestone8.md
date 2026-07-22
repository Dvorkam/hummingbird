> **Status: Active** — pre-written 2026-07, three milestones ahead but spec-driven
> (cookies/storage are RFC-shaped and nearly independent of M6/M7 discoveries).

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
