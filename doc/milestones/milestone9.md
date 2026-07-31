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
* **Scope — CORRECTED 2026-07-29 after reading the code.** The draft said
  "nothing in the engine currently bounds a request's lifetime." That is wrong,
  and the truth is more interesting:
  - `CurlNetwork` **does** set `CURLOPT_CONNECTTIMEOUT_MS 5000` and
    `CURLOPT_TIMEOUT_MS 15000` — but hardcoded, inside the *adapter*, where no
    policy layer can see or change them.
  - Because M8 took the redirect loop into the engine, **each hop is its own
    curl call**, so the 15s is per hop. With `RedirectPolicy::kMaxHops == 20`,
    a worst-case chain is **20 × 15s = 300 seconds** before anything gives up.
    That is a live bug today, on document navigation, not a future fetch bug.
  - A timeout maps to `NetworkError::CurlError`, indistinguishable from DNS
    failure or connection refused. So the M8 error page cannot say "timed out",
    and fetch could not reject with a distinguishable reason.
  - Nothing is configurable, so a test would have to wait 15 real seconds.

  So the work is not "add a deadline" but **move the deadline to the engine
  request seam and give it a name**: a whole-request budget spanning the entire
  redirect chain (not per hop), a `NetworkError::Timeout` variant, injectable
  limits so tests do not sleep, and the error threaded through to the M8
  network-error page for navigations and to a distinguishable fetch rejection.
* **Ordering — do this BEFORE 9.1.1.** It is pure engine/adapter work, testable
  with no script engine at all, and it fixes a bug that already ships. Landing
  fetch first would build promise rejection on top of a 300-second worst case
  with an unnameable error, which is exactly the "replaces one silent hang with
  another" trap this story exists to avoid. 9.1.1's own acceptance needs the
  `NetworkError` variant this story adds.
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
* **Scope correction made while building (2026-07-29).** The story's acceptance
  criterion names a *reload*, but nothing in the engine distinguished one: F5 just
  re-navigated to the current URL. Once a cache exists that is not a detail — a
  page with `max-age=3600` becomes unrefreshable, so **the cache would have taken
  away the only control the user has over it**. The reload path
  (`CachePolicy`/`Tab::reload`/`Tab::hard_reload`) is therefore part of this story
  rather than a follow-up, at **two** levels: a normal reload revalidates the
  document only, a hard reload (Ctrl+Shift+R) bypasses the cache for its
  subresources too. Making F5 revalidate everything is the tempting version and is
  wrong — it is what browsers did until ~2017 and abandoned for being slow enough
  that users stopped using reload.
* **Two things the story understated.** `s-maxage` is not a nicety to skip — both
  proof endpoints send it, and Wikipedia's is 14 days against a `max-age` of 5
  minutes, so honoring the wrong one is a visible staleness bug. And `Age` is not
  an edge case: Wikipedia's responses arrive with `Age: 11914` against
  `max-age=300`, i.e. **stale before they are ever stored**, so revalidation is the
  normal path against a CDN rather than the exception.

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

> **What 9.4 actually delivered, so the section title does not mislead
> (recorded 2026-07-31).** "Extension Follow-Through" reads like the extension
> platform advanced substantially here. It did not, and the story text is why:
> the deliverable is stated as *the hook*, and the milestone North Star measures
> *blocked requests and bytes avoided* — both engine-behavioural. So the split is
> roughly:
>
> * **Engine feature (most of it):** `Core::RequestFilter`, the `send_request`
>   gate, `ResourceState::Blocked`. `RequestFilter` deliberately does not know
>   extensions exist; a user-level block list could populate it tomorrow.
> * **WebExtensions platform (the smaller part, but load-bearing):** caller
>   identity through `bind_extension_host` and enforced `permissions` — before
>   this, *no* API could be gated at all — plus MV3 manifest parsing for
>   `declarative_net_request.rule_resources` and a `browser.*` API in the MV3
>   shape.
>
> None of that is a wrong turn: a native matcher **is** how
> `declarativeNetRequest` is implemented in Chrome and Firefox, so the engine
> work is the API, not a detour from it. But it is worth stating that
> `declarativeNetRequest` is the one WebExtensions API that teaches you *least*
> about hosting extensions, because it is declarative specifically so that
> extension code never runs on the request path. Anyone who reads this section
> expecting content scripts, `runtime` messaging, `storage.local` or
> extension-hosted pages should go to **`T-EXT-PLATFORM-FOUNDATION-1`**, which is
> where those live. They were kept out of M9 on purpose: they are not networking
> work and do not belong in "The Fetcher".

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

**Reshaped again at implementation (2026-07-31).** Three changes to the scope above,
each recorded with its reason so the divergence from the kickoff text is deliberate
rather than drift:

1. **Static rulesets replace persistence.** The scope says rule sets "need to survive
   a restart, which the host has no persistence for today", and treats that as a
   subsystem to build. It isn't one. Background scripts run at *every* startup, so
   anything registered from JS is re-registered anyway; the only thing persistence
   would buy is survival of *dynamic* rules added mid-session. MV3 does not solve the
   main case that way either — it uses `declarative_net_request.rule_resources`, a
   static JSON ruleset named in the manifest and loaded by the **host**, with no
   script involved. Taking that path gets "rules survive a restart" correctly and for
   free, produces the list format 9.4.2 needs regardless, and removes an ordering
   coupling between background-script startup and the first page's subresources. The
   JS API stays, as a thin **session-scoped** dynamic layer documented as not
   persisted.
2. **`main_frame` is declared but never matched.** Blocking a top-level navigation
   needs a "blocked by extension" interstitial, or it renders as a network error or a
   blank tab — a UX story, not this one. An ad-blocker does not need it. The
   destination enumerator exists so the vocabulary is complete; M9 refuses to match
   it.
3. **The permission gap is a plumbing gap, not an oversight.** `permissions` is
   unenforced because `IExtensionApiHost` has no caller identity —
   `insert_css(tab_id, css_text)` cannot know who is asking, and
   `bind_extension_host(this)` binds one host to every context. There is nowhere to
   put the check. So this story plumbs the extension id through
   `bind_extension_host` first (sound and cheap, because there is already one QuickJS
   context per extension), and gates `insert_css` with it too — closing the existing
   hole rather than shipping a gated API next to an ungated one.

**Where the matcher lives.** `core/net/RequestFilter.h`, not the extension host.
`ResourceLoader` already holds three profile-wide policy objects of exactly this
shape (`CookieJar`, `HttpCache`, `IdentityPolicyStore`); this is the fourth. That
placement is what makes the matcher unit-testable with no script engine, keeps the
network path ignorant of extensions, and leaves the door open for a user-level block
list that is not an extension at all.

**Where the gate goes.** The top of `send_request`, before `apply_deadline` — so a
blocked request costs no cookie header, no cache lookup and no transport. Critically
it therefore matches **per redirect hop**, for the reason already written into that
function for CORS: *"a check applied only to the first request is not a check"*. Ad
networks redirect constantly, so the argument transfers verbatim.
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
     *Care needed (2026-07-31): the request count is real, but **we never fetched the
     blocked resource, so we do not know its size** — a "bytes avoided" counter
     computed inside the filter would be fabricated data. Two honest routes, and the
     implementation must use them: the fixture test reports the bytes the **mock
     server** knows it did not serve, and the real-page claim comes from an actual
     before/after run with the extension toggled, which is what "avoided" means
     anyway. `RequestFilter` therefore counts requests and deliberately exposes no
     byte counter.*
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
* **The live manual gate already exists (2026-07-29).** `example.dev/m9` fetches
  `api.hnpwa.com/v0/news/1.json` and the Wikipedia REST summary and renders both,
  **user-confirmed working**. So the North Star claim — *"the browser shows live
  data it fetched itself"* — is demonstrable now, well ahead of schedule. What is
  still missing is this story's actual deliverable: the **CI-runnable** version
  against a mock server. A manual gate proves it works today; only the harness
  stops it silently breaking. Do not let the working demo make this story feel
  done.
* **Note for 9.2.1:** those two live fetches are cross-origin and currently
  unchecked. Both endpoints answer `Access-Control-Allow-Origin: *` (re-verified
  2026-07-29), so they must keep working after CORS lands — which makes that same
  card a ready-made "CORS allows what it should" check, alongside the mock
  server's "CORS blocks what it should".
* **Kickoff check — DONE 2026-07-29.** Both endpoints probed live with a
  cross-origin `Origin: https://example.dev`. **Both are CORS-open**, so the
  cross-origin manual gate is viable and the kickoff's flagged risk is closed.
  The responses also hand several later stories real-world shapes to build
  against, which is worth more than the yes/no answer:

  | Observed | Story it informs |
  |---|---|
  | Both: `Access-Control-Allow-Origin: *` | 9.2.1 — and note the consequence below |
  | Wikipedia: `Access-Control-Expose-Headers: etag` | 9.2.4 — a live example of the safelist plus one named header |
  | Wikipedia: `Access-Control-Allow-Methods: GET,HEAD` and a long `Allow-Headers` list, answered on `OPTIONS` | 9.2.1 preflight |
  | Wikipedia: **no `Access-Control-Max-Age`** on the preflight | 9.2.2 — there is nothing to cache beyond the spec default, so preflight caching buys less than assumed; it stays P1 |
  | HNPWA: `Cache-Control: max-age=3600`, strong `ETag`, `Last-Modified` | 9.3.1 — a real freshness + revalidation target |
  | HNPWA: **`Vary: x-fh-requested-host, accept-encoding`** | 9.3.2 — a live `Vary` on a header the engine actually sends |
  | Wikipedia: `Cache-Control: s-maxage=1209600, max-age=300`, **weak** `ETag` (`W/"…"`), `Age: 11914` | 9.3.1 — weak-vs-strong validator comparison, and `Age` participates in freshness |

  **Consequence of `Allow-Origin: *` on both:** per spec a credentialed request to
  an origin answering `*` is rejected. So 9.2.1's credentials-mode acceptance can
  only be exercised against the mock server — the live gate proves the
  non-credentialed path. That is a limit of the live gate, not of the tests.

* **Story 9.5.2: Missing-API Telemetry Triage** *(new, filed 2026-07-30)*
* **Goal:** turn the missing-API counters into a triaged backlog, which is what
  they were built for.
* **Why it exists:** "Missing-API telemetry from the proof-target runs is triaged
  into the M12 backlog" has been a **Milestone 9 Done-When bullet since kickoff
  with no story behind it**, so nothing was going to make it happen. It also
  **gates 9.1.2**, whose entire pull-in trigger is "only if telemetry reports it" —
  that question cannot be answered until someone runs the triage, so 9.1.2 is not
  really a scheduling decision yet, it is a blocked one.
* **Scope:** run the proof targets (the `example.dev/m9` live cards plus a handful
  of real pages) and collect what `record_missing_api` reports; file each
  recurring hit as an M12 story or fold it into an existing one; and decide 9.1.2
  on the `XMLHttpRequest` count specifically.
* **Known blind spot this story must state, not just work around:** the fail-soft
  prelude reports **globals that get called** — `XMLHttpRequest`, `matchMedia`,
  `localStorage`. It cannot see a missing **property**, because reading
  `document.body` yields `undefined` and throws at the *use* site with no report.
  `T-DOM-DOCUMENT-BODY-1` was found by a fixture author, not by the counters that
  exist to find exactly that. So the triage output is a **lower bound**, and the
  story should say so rather than let M12 read the number as complete.
* **Acceptance:** a written triage in `doc/TODOs.md` or `milestone12.md`, and a
  yes/no on 9.1.2 with the count behind it.
* **Tests:** none — this is an analysis story.

#### Findings (2026-07-30)

The analysis ran and produced two results that matter more than any count:

**1. The instrument was write-only, and is now readable.**
`IScriptEngine::missing_apis()` had **no caller anywhere in `src/`** — only a
unit test read it — and the list is cleared on every navigation. So the data was
recorded per document and discarded. `DocumentPipeline` now emits it at document
end, next to the compatibility summary, as a greppable
`[missing-api] <url>: N unimplemented API(s) touched: …` line. Unlike the compat
summary it reports a **single** occurrence, because one page touching an
unimplemented API once is the whole signal, whereas that summary exists to
collapse repeats.

**2. The instrument can see exactly two things, which invalidates how M12 was
meant to be scoped.** The fail-soft prelude reports four names —
`XMLHttpRequest`, `matchMedia`, `localStorage`, `sessionStorage` — and the latter
two have been **implemented since 8.2.2/8.2.3**, so their stubs never fire in the
app. The real observable surface is therefore `XMLHttpRequest` and `matchMedia`.
Nothing else reports anything: not `IntersectionObserver`, `MutationObserver`,
`customElements`, `WebSocket`, `requestIdleCallback`, `getComputedStyle`, and not
any missing **property** (the `document.body` class of gap — see the blind spot
above).

  **Consequence for 9.1.2, and it is not the obvious one.** XHR's pull-in trigger
  is "only if telemetry reports it" — but XHR is one of only two things the
  telemetry *can* report. That trigger therefore selects for **visibility, not
  demand**: it would fire for XHR and could never fire for the twenty APIs a real
  framework page is likelier to want. Treating a hit as evidence of relative
  importance would be reading a selection artefact. **9.1.2 stays unscheduled**,
  and its trigger is restated: implement XHR when a *specific proof target* needs
  it, which is a question about the target, not about the counters.

  **Consequence for M12.** "M12's scope is derived from missing-API telemetry at
  kickoff" cannot hold while the instrument sees two names. Filed
  `T-JS-MISSING-API-COVERAGE-1` to broaden the stub list before that kickoff;
  until it lands, any triage output is a **lower bound** and must be labelled one.

#### The live sweep ran (2026-07-30, user-supplied log over wikipedia.org + hn.algolia.com)

**It found almost nothing, and that is the finding.** 522 log lines produced
exactly **two** `[missing-api]` reports, both `navigator.userAgent`. Meanwhile
the same run shows scripts dying:

| Observed | What it means |
|---|---|
| `ReferenceError: Element is not defined` (wikipedia's bundle) | A DOM interface object we do not expose. **Not on the 14-name stub list** and never would have been. |
| `TypeError: cannot read property 'indexOf' of undefined` (Google Tag Manager) | `navigator` existed but its shape was wrong — the string fields were missing, not empty. |
| `ReferenceError: aa is not defined` | A minified bundle's own local. Noise. |
| `Error: Invariant failed` | A framework's own assertion, downstream of an earlier failure. Noise. |

**So the stub-list approach has a ceiling: it can only report gaps somebody
predicted.** The loudest real signal was in a channel that was being logged and
thrown away — `[script] eval failed … ReferenceError: X is not defined` names
the missing global exactly. That is now harvested in
`QuickJSScriptEngine::eval` and recorded as `X (ReferenceError)`, deliberately
suffixed: a stub hit means the page carried on without the feature, a
ReferenceError means the script **died** there and nothing after it ran. A
triage that merged them would rank a fatal gap alongside a handled one. Mangled
minified locals are filtered by length, and non-ReferenceError failures are
ignored.

`navigator` also gained `appVersion`/`appName`/`product`/`vendor`/`doNotTrack`/
`hardwareConcurrency`/`maxTouchPoints`, all string- or number-typed, because the
sweep proved that **a stub existing is not enough if its shape is wrong**.

**Two real bugs fell out of a sweep that was looking for something else** — both
filed P1, neither related to telemetry:
`T-HTML-ATTR-ENTITY-DECODE-1` (character references are decoded in text but
**not in attribute values**, so `href="/wiki/Sam_&amp;_Max"` really requests
`Sam_&amp;_Max` and 404s on a real article) and `T-NET-DATA-URL-1` (`data:`
URLs are handed to curl instead of decoded, so every inline SVG icon costs a
failed network request and renders nothing).

**Still outstanding:** a re-run now that ReferenceErrors are harvested. The first
sweep measured the instrument more than the pages; the second one should
actually enumerate what real sites want.

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

**Re-triaged 2026-07-30**, once every P0 had landed ahead of schedule. The question
this table now answers is not "what is parked in M9" but "what does M9 still owe."

| Ticket | P | Note |
|---|---|---|
| `T-DOM-DOCUMENT-BODY-1` | P1 | **Kept.** `document.body`/`head`/`documentElement` are unbound. A binding gap in the exact layer M9 spent the milestone building on: fetch continuations mutate the DOM, and the most common way a page does that throws. Found by 9.5.1. |
| `T-NET-IDENTITY-UI-1` | P1 | **Kept.** Since `c18dc9a` removed the `Ctrl+Shift+U` instructions from the README, the only route to a *shipped* feature is an undocumented keyboard shortcut. A shipped feature reachable only by a secret is a hole, not a nicety. **9.3.2 raised the stakes**: the toggle now also forces a hard reload, so an invisible control grew a second invisible effect. |
| `T-COOKIE-CONFORMANCE-VECTORS-1` | P2 | **Kept.** Its stated precondition ("do this after 8.3.1/8.1.3 so redirect behavior is settled") is now met. Cheap, and it gives 9.0.3 a tracked number. |
| `T-NET-IDENTITY-AUTOOFFER-1` | P2 | **Kept, gated.** Phase 2 of the identity work; depends on `T-NET-IDENTITY-UI-1` for its surface, so it lands with that story or slips with it. |

**Deferred at the 2026-07-30 re-triage** (re-tagged in `doc/TODOs.md`; each is
ordinary work that M9 was merely the parking lot for):

| Ticket | Now | Why there |
|---|---|---|
| `T-HTML-PRESENTATIONAL-TAGS-1` | M10 | `<center>`/`<u>` are UA style defaults plus an inline-flow bug (`<u>` laid out as a block breaks the line it sits in). M10 owns the inline box model and the UA defaults; fixing it here would be a drive-by in someone else's layer. |
| `T-FONT-WOFF2-1` | M11 | M11 §11.4 is the text-rendering/font milestone. It also vendors a new dependency (`woff2` + `brotli`), which belongs with the rest of the font stack rather than alone. |
| `T-NET-CLIENT-HINTS-1` | M10 | Pairs with `T-NET-EFFECTIVE-REQUEST-HEADERS-1`, already M10 P3. Both are the same question — does the cache key match the headers actually sent — and splitting them across milestones is how one gets fixed and the other forgotten. |
| `T-STORAGE-DOT-ACCESS-1` | M12 | `localStorage.foo` is a convenience idiom, not a correctness hole, and it needs exotic-object handlers done carefully. M12 owns the JS surface pages assume. |

**Re-homed at kickoff** (were tagged `[M8]`, do not belong here):
`T-FOCUS-MUTATION-SYNC-1` → M11 (focus system), `T-EVENT-SPEC-GAPS-1` → M12 (event
constructors and listener options are framework-surface items).

**Correction (2026-07-30):** this table listed `T-FORM-PASSWORD-MASK-1` and
`T-FORM-INPUT-PASTE-1` as M9 P1 until today. Both were re-homed to M11 at the
kickoff triage and have been full stories there — **11.1.2 and 11.1.1** — the whole
time; `doc/TODOs.md` recorded the move and this table did not. That is the same
failure the kickoff found at M8→M9 scale ("nine stories were still tagged `[M8]`"),
in miniature: **an index that is not regenerated from its source drifts from it.**
Both rows are removed rather than restated.

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
- [x] 9.0.3.1: Registrable Domain From A Public Suffix List *(new
      `core/net/PublicSuffix.{h,cpp}`: `public_suffix`, `is_public_suffix`,
      `registrable_domain`, over a curated ~80-rule table written in
      publicsuffix.org syntax and matched with the full PSL algorithm — longest
      match wins, `*` matches one label, `!` is an exception, and the implicit `*`
      default rule covers every single-label TLD without a table entry.
      `is_same_site` now compares registrable domains; `parse_set_cookie`
      implements §5.3 step 5, so `Domain=co.uk` is rejected unless the request host
      IS `co.uk`, and then only as host-only. Tests:
      `tests/core/PublicSuffix.test.cpp` (8 cases incl. wildcard/exception rules
      and case handling) + `CookieMatchTest.SameSiteUsesTheRegistrableDomain` +
      `CookieParseTest.DomainAttributeCannotBeAPublicSuffix`.
      **Superseded the same day** — see below.)*
- [x] 9.0.3.1b: Full public suffix list, CI-maintained *(the curated table was
      replaced with the real publicsuffix.org list, 10,239 rules → 10,409 entries.
      Reason: a missing multi-label registry fails **open** — `a.com.pe` and
      `b.com.pe` would have been judged one site — and 9.2.1 CORS is about to lean
      on `is_same_site`. Bundled at build time, never fetched at runtime (a
      browser that must reach the network before it can evaluate cookie policy has
      a bootstrap problem, and a security input fetched over the network has an
      author you did not choose). Pinned to an upstream **commit**, recorded in the
      generated header as the single source of truth; no timestamp, so
      regeneration is byte-reproducible. `scripts/update_public_suffix_list.ps1`
      generates data + vendors the list's own conformance vectors from the same
      commit. Lookup is now O(labels): each candidate suffix is binary-searched in
      a sorted table. **IDN gap found and closed by the upstream vectors:** the
      engine has no IDNA layer, so a Unicode hostname reaches the cookie code as
      written; each internationalized rule is therefore emitted twice, punycode
      and Unicode, or those hosts would fall through to the permissive default.
      CI: a daily job opens a refresh **PR** (never auto-merges — upstream is
      community-submitted, so a human reviews a security boundary), a per-PR check
      that the generated file matches its source, and a **release gate** that
      fails a tag build while the bundle is behind upstream. Tests: all 78 upstream
      vectors pass, plus `InternationalizedHostsMatchInEitherForm`.)*
- [x] 9.0.3.2: Cookie Jar Limits *(`CookieJar::kMaxCookieBytes`/`kMaxPerDomain`/
      `kMaxTotal` = 4096/50/3000. An oversized cookie is refused rather than
      stored — including a replacement, so a cookie cannot grow past the cap by
      being re-set. `evict_for()` runs §5.3 step 11 only when a cap is about to be
      exceeded: expired cookies first, then the least-recently-used of the domain,
      then of the whole jar. `load_from` trims through the same path, because the
      file on disk is as untrusted as the network that filled it. LRU needed a
      last-access time, so `cookies_for`/`cookie_header_for`/`script_visible_cookies`
      are **no longer const** — per §5.3, retrieval is what makes a cookie recently
      used. `Cookie::last_access` **is persisted**: the jar file format went to
      `HBCOOKIES 2` and a v1 file is discarded rather than migrated, logging the
      user out once. That was first implemented the other way — runtime-only, to
      avoid the breaking change — and revisited at the user's direction: pre-alpha,
      the correct format wins over preserving a saved session, and the loss is
      called out in the CHANGELOG. Tests:
      `CookieLimitsTest.*` — the LRU case deliberately evicts the **newest** cookie
      by creation time, the only one no request had touched, so it cannot pass
      under a FIFO implementation.)*
- [x] 9.0.3.3: Cookie Charset Validation *(`parse_set_cookie` now requires the
      name to be an RFC 2616 token, and rejects CTLs/DEL/`;` in the value and in
      the Domain and Path attributes — every field the jar re-serializes into both
      a `Cookie` header and its own TSV file. **Deliberate deviation:** §4.1.1's
      cookie-octet also excludes SP, `,`, `"` and non-ASCII; real sites send all
      of those and none can terminate a header or TSV field, so only the injection
      set is enforced — what shipping browsers do. `CookieJar`'s `is_serializable`
      skip, which existed only because parsing did not validate, is now an
      invariant guard at the serialization boundary and logs a warning instead of
      a debug line, because reaching it means a parse bug. Tests:
      `CookieCharsetTest.*`, including a jar-level case that a rejected
      `Set-Cookie` never reaches the `Cookie` header.)*

P0: fetch + CORS (North Star)
- [x] 9.1.3: Request Deadlines *(ran ahead of 9.1.1. `NetworkRequestOptions` now
      carries per-call connect/total deadlines; `CurlNetwork` uses them and keeps
      its old hardcoded values only as a backstop for a direct `INetwork` user.
      `ResourceLoader::RequestDeadlines` owns the policy, and `RedirectChain`
      carries a deadline set on the first hop and unchanged after, so each hop
      gets only what is left and a hop starting with nothing left is never issued.
      Connect is clamped to the remaining total. `NetworkError::Timeout` is
      distinct, with its own error-page wording — "took too long … trying again
      often works" rather than "didn't respond", which reads as a wrong address.
      The clock is injectable, so tests prove a chain gives up part-way without
      sleeping. Tests: `RequestDeadlineTest.*`; the headline one was **verified to
      fail under the old per-hop behaviour** (21 requests and `TooManyRedirects`,
      instead of 4 and `Timeout`). **Still open for 9.1.1:** the fetch promise's
      distinguishable rejection — the `NetworkError` variant exists, nothing
      surfaces it to JS yet.)*
- [x] 9.1.1: fetch() Core *(the full vertical: new
      `core/platform_api/ScriptFetch.h` types, `IScriptHost::start_fetch` +
      `resolve_url`, `IScriptEngine::settle_fetch`, a QuickJS binding that creates
      the promise capability and stashes resolve/reject under the host's id, a JS
      prelude wrapping the raw payload into `Response`/`Headers`, and
      `ResourceLoader::fetch_for_script` riding `send_request` so a fetch inherits
      cookies, identity, referrer, redirects and the 9.1.3 deadline. **The async
      shape:** the transport answers on a pool thread, so the Tab QUEUES the
      result and settles it on the main thread in `tick()` — the same pattern
      document/subresource loads use, not a second one. Spec-shaped where it
      counts: only a network error rejects (404 resolves with `ok === false`), a
      timeout rejects as `TimeoutError` (the JS-visible half of 9.1.3), a body is
      single-use. **Teardown:** `reset_bindings` frees the pending resolve/reject
      *without calling either*, and the Tab drops queued responses via a
      generation counter — both halves are needed. The never-settling stub is
      gone, so `fetch` also left the missing-API telemetry list. Tests:
      `FetchTest.*` (8). Demo: example.dev/m9 fetches `/api/news` from a new stub
      JSON endpoint shaped like `api.hnpwa.com`.
      **NOT yet done, and deliberately:** no `Request` class, no `AbortController`,
      no streaming, no `FormData`; cross-origin requests are currently
      **unrestricted** — CORS is 9.2.1 and until it lands a page can read any
      origin it can reach.)*
- [x] 9.2.1: Request Classification + Enforcement *(new `core/net/Cors.{h,cpp}`,
      pure functions over a request and a set of response headers, so the whole
      matrix is testable without a server. Same-origin fetches bypass CORS
      entirely and get no `Origin` header. Cross-origin ones send `Origin`, and
      the response is vetted before any part of it reaches the page — a block
      discards **status, headers and body together**, because "that origin
      answered 401" is itself cross-origin information. The page cannot tell a
      block from a network failure; the reason is logged for the developer only.
      Preflight (OPTIONS) for non-simple requests, checked for method and every
      non-safelisted header, and **the real request is never sent when it is
      refused** — which is the point when that request would have deleted
      something. Engine-added headers (User-Agent, Referer, Sec-CH-*) never
      trigger a preflight. Credentials mode drives the cookie jar via a new
      `CookieRequestContext::credentials_allowed`: a cross-origin fetch is
      anonymous by default and **neither sends nor stores cookies**, since
      storing would let a request the page cannot even read still plant one.
      Preflights are never credentialed. Tests: `CorsTest.*` (10, the matrix) +
      8 `FetchTest.*` integration cases through the real loader.
      **Learned while testing:** `credentials: 'include'` is necessary but not
      sufficient — SameSite is a separate gate that runs first, so a default
      (Lax) cookie stays home on a cross-site fetch regardless. Pinned in the
      credentials test.)*
- [x] 9.2.3: CORS Across Redirect Hops *(the check moved INTO
      `send_request`'s per-hop path, joining the Cookie header, SameSite context,
      Referer and per-origin identity that M8 already recomputes there — so the
      seam this story needed was already built. `RedirectChain::CorsState` carries
      origin, credentials and two sticky flags. **`active` is never cleared**, so a
      chain that wanders off-origin and returns cannot launder its way back into
      same-origin treatment; **`tainted`** makes every hop after a cross-origin
      redirect present `Origin: null`, so the next server must opt in to an opaque
      origin rather than to the page that started it. A **preflighted** request
      refuses to follow a redirect at all (the server agreed to the request it was
      asked about, not to wherever it points next), and a preflight itself may not
      be redirected. A same-origin URL that 302s off-origin **becomes** a CORS
      request — enforcement keyed off the initial URL alone would miss it
      entirely. New `NetworkError::CorsBlocked` lets the redirect loop abandon a
      chain mid-flight. The duplicate check in `deliver` is gone: one place,
      per hop. Tests: 5 new `FetchTest.*` redirect cases; **verified that 3 of
      them fail with a first-hop-only check**, which is the bug this story
      exists to prevent.)*
- [x] 9.2.4: Response Header Exposure *(the other direction from 9.2.1: that asks
      what the SERVER allows for the request, this asks which response headers the
      PAGE may observe. `Cors::filter_exposed_headers` keeps the safelist plus
      whatever `Access-Control-Expose-Headers` names, minus a **forbidden** set
      (`Set-Cookie`/`Set-Cookie2`) that is unreadable however the server asks —
      that category exists precisely because a server naming `Set-Cookie` has
      misunderstood, and honouring it would hand the page another origin's
      session. `*` in Expose-Headers means "everything not forbidden" only for an
      anonymous request; for a credentialed one the spec reads it as the literal
      header name. Applied at the same per-hop seam as 9.2.3, **after** the jar
      has taken any `Set-Cookie` — the browser stores that cookie, the page just
      never reads it. Same-origin responses are not filtered. **Note:** the story
      listed six safelisted headers; the spec has seven (it omitted
      `Content-Length`), and seven is implemented. Tests: 4 `CorsTest.*` +
      2 `FetchTest.*`; verified the `Set-Cookie` guard is load-bearing by
      disabling it and watching the test fail. Demo: the live Wikipedia card now
      prints which headers it may read — `etag` only because Wikipedia names it,
      `content-type` because it is safelisted, `age` not at all.)*

P0: Cache (North Star)
- [x] 9.3.1: Cache Core + Revalidation *(new `core/net/CacheControl.{h,cpp}`
      (pure policy, mirroring `Cookie.cpp`) + `core/net/HttpCache.{h,cpp}` (the
      store, mirroring `CookieJar.cpp`), applied at the same per-hop seam in
      `send_request` that 9.2.3 established. **The cache sits INSIDE the redirect
      loop**, so individual hops are cached and a stored 301 saves the whole chain
      next time. A fresh hit is fed through the very same response path a live
      answer takes — CORS verdict, exposure filter, redirect following — because a
      cache that bypassed those would be a way to launder a response past the
      checks that admitted it.
      **9.3.1 is deliberately conservative rather than fast:** it refuses to store
      anything carrying `Vary`, anything carrying `Set-Cookie`, and any response to
      a request that sent `Cookie`/`Authorization`. Each of those is a cache KEY in
      9.3.2; storing them first and keying them later would mean shipping a
      knowingly wrong cache for the length of one commit. `s-maxage` is parsed and
      **deliberately ignored** — it addresses shared caches, and reading Wikipedia's
      `s-maxage=1209600` instead of its `max-age=300` would cache an article for 14
      days. No heuristic freshness either: a response with only validators is stale
      immediately and revalidates, which still saves the body.
      **Reload had to be built as part of this story.** Without it a page with
      `max-age=3600` would be unrefreshable, so the cache would have removed the
      one control the user has over it. New `ResourceLoader::CachePolicy` +
      `Tab::reload()`/`hard_reload()` + `BrowserApp::reload_and_reflect_url(hard)`.
      **Two levels, and they deliberately differ in reach:** F5/Ctrl+R revalidates
      the *document* only; Ctrl+Shift+R (or Ctrl+F5) bypasses the cache for the
      document *and* every subresource. The first draft of this had F5 revalidating
      subresources too — that is pre-2017 browser behaviour, which Chrome and
      Firefox both abandoned because a page with fifty assets turned one keystroke
      into fifty conditional requests and made reload the slow way to reload.
      Caught in review before commit.
      Tests: 14 `CacheControlTest.*` + 12 `HttpCacheTest.*` + 13
      `HttpCacheIntegrationTest.*` (934 total green). **Verified two guards are
      load-bearing** by disabling each and watching the right test fail: storing
      the CORS-*filtered* headers makes a cross-origin resource work once and then
      break silently on reuse (the filter drops `Access-Control-Allow-Origin`, so
      the re-check refuses a response the server allowed), and dropping the reload
      policy makes F5 serve from cache.
      **Note:** `Cache-Control: private` is refused, which is stricter than the RFC
      — `private` addresses shared caches and a per-profile memory cache is the
      boundary it protects. Kept strict for M9 because the credentialed-response
      rule that would make storing it safe is 9.3.2's; filed as a follow-up.
      Demo: a new card on `example.dev/m9` with **server-side hit counters**, so the
      cache is shown rather than asserted — the body keeps saying "served #1" while
      the server's `full` count stays at 1, then `revalidated` climbs instead once
      the entry goes stale, and a `no-store` control endpoint climbs every click.
      **Demo follow-up after the user checked it (2026-07-29):** the API/no-store
      halves worked, but F5 and Ctrl+Shift+R were indistinguishable *and the card
      claimed otherwise* — nothing on the demo site is cacheable (the stub sends no
      `Cache-Control`, and `/m9` had no external subresources), so there was
      genuinely nothing to see. Added a cacheable stub stylesheet
      (`/api/cache-demo/style.css`, `max-age=300` + ETag) with its own counters,
      reported on page load, which is the only way the reload levels become
      visible. Building it surfaced a **latent routing bug**: subresources fell back
      to the stub only when there was NO primary transport
      (`allow_fallback_network`), so a demo page's absolute-path subresource went to
      curl and failed DNS — the same split that made a fetch to example.dev
      unresolvable, in the one request path that had never been given the rule.
      Fixed with the shared `is_builtin_demo_url` check and pinned by a test
      verified to fail without it. The card also now states which counter a hard
      reload moves: **`full`, never `revalidated`** — refusing to ask "is my copy
      still good?" is the point of it, so it can never earn a 304. The redundant
      "ask the server" button is gone; both fetch buttons refresh the counters.)*
- [x] 9.3.2: Cache Key Correctness — `Vary`, `private`, Credentials *(the three
      refusals 9.3.1 shipped on purpose became actual cache KEYS. `HttpCache::Entry`
      carries a **secondary key** (RFC 9111 §4.1): the `Vary` field names the
      response declared, the values THIS request sent for them, and a
      `CredentialsClass`. One URL now holds several variants, each answering only
      its own request, capped at `kMaxVariantsPerUrl = 8` with eviction drawn from
      that URL's own variants — otherwise `Vary` is an unbounded-entry hole through
      a single address.
      **Decisions the story asked for, and what they turned on:**
      *Credentials* are keyed, not refused: a credentialed response never answers an
      anonymous request or the reverse. The cookie **value** is deliberately NOT in
      the key — that would invalidate everything on any cookie change, and it is not
      what browsers do; a server whose answer really depends on who is asking says
      `Vary: Cookie`, which the secondary key then honors.
      *`Cache-Control: private` is now STORED*, reversing 9.3.1. The directive
      addresses **shared** caches, and a per-profile in-memory cache is exactly the
      private cache it permits; refusing it cost hits on the logged-in pages caching
      helps most and bought nothing, because what makes storing it safe is the
      credentials class. `Vary: *` is the one new blanket refusal — a server
      admitting it varies on something it will not name means no key can be correct.
      *`Set-Cookie` is stripped before the entry is written* rather than the response
      being refused, so the body caches and the session token does not.
      **Identity is handled twice, on purpose.** For servers that say
      `Vary: User-Agent`, the secondary key separates the modes. For those that do
      not, Ctrl+Shift+U now triggers a **hard** reload — a normal one would serve the
      other mode's cached page and make the toggle look broken, which is the opposite
      of why the user pressed it. Keying UA unconditionally was the alternative and
      would halve the cache for every site to fix the few that declare it.
      Tests: 20 `CacheControlTest.*` + 18 `HttpCacheTest.*` + 18
      `HttpCacheIntegrationTest.*` (948 green). **Both halves of the key verified
      load-bearing** by disabling each: ignoring `Vary` fails 4 tests, ignoring the
      credentials class fails 2.
      **Learned while testing:** a test cannot vary `User-Agent` by setting it on the
      request — `send_request` overwrites it from the identity store, which is correct
      (it is a forbidden header for fetch, and the engine owns identity). The only
      honest way to exercise it is to toggle the store, which is also what the user
      does. Filed two gaps rather than half-fixing them:
      `T-NET-CACHE-PARTITION-1` (no top-level-site partitioning — a cross-site
      timing oracle that applies to every entry) and
      `T-NET-EFFECTIVE-REQUEST-HEADERS-1` (the transport owns `Accept-Encoding`, so a
      response varying on it is keyed on our empty value — consistent today, wrong in
      principle).)*

P0: Guardrails
- [x] 9.5.1: API-Render Harness *(new `tests/engine/ApiRenderFlow.test.cpp` +
      `tests/fixtures/api_render/{story_list,summary}.html`, driving the real Tab
      against a `MockApiServer` fake `INetwork`. Six cases: the same-origin list
      render, the cross-origin summary allowed and blocked, a cached second visit,
      a cached cross-origin response re-passing CORS, and F5.
      **Every assertion is on painted text or on what the server saw**, never on
      the DOM or on the cache's own stats — a DOM assertion passes while a missed
      rebuild leaves the screen stale, and a cache that reports a hit while still
      issuing the request can only be caught from the transport side. The fixture
      pages build one element per story rather than one blob of text, so the
      fetched data has to survive DOM → style → layout → paint before a test can
      see it.
      **All three guards verified load-bearing** by disabling each and watching
      the right tests fail: no cache lookup → 3 fail, no CORS response check → the
      blocked case passes a cross-origin body to the page, no
      rebuild-on-fetch-settle → **all 6** fail (the render half is what every one
      of them rides on). 954 tests green, up from 948.
      **Gap found while writing it:** `document.body` is not bound at all — the
      summary fixture's `document.body.appendChild` threw a `TypeError` that the
      page's own `.catch` then reported as a network failure, which is exactly how
      it presents in the wild. Filed as `T-DOM-DOCUMENT-BODY-1` (P1) and the
      fixture pointed at a pre-existing element rather than worked around.
      `HeadlessTabHarness` grew `identity_store` + `http_cache` parameters so a
      test can own the cache it asserts on.
      **Deliberately still manual:** the live run against `api.hnpwa.com` and
      `en.wikipedia.org`, which stays the `example.dev/m9` demo card.)*

P0: Reopened 2026-07-30 by a crash found in manual browsing
- [x] T-RESOURCE-REF-1: downstream layers hold resource *references*, not payload
      pointers *(the fix for `T-CRASH-IMAGE-HEAVY-PAGE-1`, a proven
      use-after-free that killed the browser on seznam.cz. The render tree and
      the retained display list both cache `const ImageBitmap*` into
      ResourceStore memory, which the store frees at four points with no
      invalidation. Full reasoning, the two rejected designs and the blast radius
      are in `doc/TODOs.md`; the short version is that the store keeps owning
      decoded pixels and consumers hold refs resolved at the point of use, which
      also removes the staleness the current re-pointing step exists to paper
      over.)*

P1: Re-triaged 2026-07-30 (every P0 landed, and early). Ordered.
- [x] T-DOM-DOCUMENT-BODY-1: `document.body`/`head`/`documentElement` *(one
      enum-keyed port method `IScriptHost::document_part(DocumentPart)` rather
      than three near-identical virtuals — the shape `StorageKind` already uses,
      and the alternative grows that interface by one virtual per property
      forever. The three JS properties share one magic getter via a new
      `define_getter_magic` helper, mirroring how localStorage's `length` is
      registered. Read-only: `document.body` is writable per spec, but replacing
      it wholesale is not something pages do, and *reading* is the gap that broke
      them.
      **Tested through the real parser, not a hand-built tree** — the entire
      question is what shape the parser produces (`root` wrapper → `<html>` →
      `<head>`/`<body>`), so a hand-assembled tree would confirm the assumption
      instead of testing it. Verified load-bearing by unbinding `document.body`
      and watching the render test fail. 959 green, up from 954.
      **Found the deeper half and filed it rather than papering over it:** this
      engine's parser does **not** synthesize the html/head/body skeleton a
      browser's tree construction guarantees, so `<p>hi</p>` really has no body
      element and `document.body` returns **null** there. Substituting the
      synthetic root would have made `document.body.tagName === 'ROOT'` and
      quietly misled every page that checks. Filed `T-HTML-TREE-SKELETON-1`
      (M10 — it owns the style/layout consequences); a `body { }` rule matches
      nothing on those documents today for the same reason.
      Spec detail kept: `document.body` falls back to `<frameset>`, because a
      frameset document has no `<body>` and null would read as "no document".
      Demo: a new card on `example.dev/m9` that appends a real element to
      `document.body` — it lands below the back-link, outside the card, because
      that is genuinely where it goes.)*
- [x] 9.5.2: Missing-API Telemetry Triage *(the analysis ran; see the Findings
      under the story. Two results: the instrument was **write-only** —
      `missing_apis()` had no caller in `src/` and the list is cleared every
      navigation — and is now emitted at document end as a greppable
      `[missing-api]` line; and it can observe **exactly two** APIs
      (`XMLHttpRequest`, `matchMedia`), because the other two stubs cover
      features implemented back in 8.2.2/8.2.3. That invalidates "M12's scope
      comes from the telemetry" until `T-JS-MISSING-API-COVERAGE-1` widens it,
      and it re-frames 9.1.2: XHR's trigger selects for **visibility, not
      demand**, since XHR is one of the only two things that can ever fire it.
      **Outstanding and needs the app** (the sandbox blocks SDL exes): the live
      sweep over real pages that turns the instrument into numbers.)*
- [x] 9.6.1: History API MVP *(`history.pushState`/`replaceState`/`state`/
      `length`/`back`/`forward`/`go` plus the `popstate` event, wired to the tab's
      existing back/forward stack. **Stage A** (hash routing) was already working
      and is now covered by the harness alongside the new routes rather than
      being replaced by them.
      **The design correction that made it work:** the first cut flagged each
      pushState ENTRY as same-document, which is not enough — going Back from a
      pushed entry lands on the DOCUMENT's own entry, which no pushState created,
      and reloading there defeats the API just as completely. Entries now carry
      the id of the document they belong to (`Tab::document_generation_`, bumped
      in `begin_navigation_session`), and traversal is same-document exactly when
      that matches what is loaded — which is how browsers decide it.
      `pushState` resolves its URL against the engine's own `location_url_`
      rather than the host resolver `fetch` uses, so two relative pushes in one
      script run chain off each other (the host's base only moves once the Tab
      drains) and the API does not depend on a resolver being wired.
      State is serialized to JSON at the call, because it has to outlive the JS
      context in the Tab's stack; an unserializable state throws rather than
      storing something the page did not ask for. Empty state is null, which is
      distinct from the string "null" a page may legitimately store.
      **Deliberate deviations, both narrow:** traversing to a *fragment* entry
      fires `hashchange` only, not also `popstate` — preserving 7.2.5's behaviour
      exactly, and the spec's both-events rule is a same-document edge case M12
      owns; and `history.go(0)` is a no-op rather than a half-implemented reload.
      Tests: 6 binding cases + 2 harness flows. The acceptance flow asserts the
      document was requested exactly once across list → detail → back → forward,
      **verified load-bearing** by forcing traversal to always reload and watching
      it fail on the "must not reload" assertion. 998 green, up from 990.
      Demo: an m9 card whose route counter lives in page script, so a reload would
      reset it to zero — the one number a page cannot fake.
      **Two follow-ups came out of manual testing, both real:** the demo server
      served only single-segment page names, so walking Forward onto a pushed
      `/m9/detail/42` from another document showed "failed to load" — correct
      browser behaviour meeting an incomplete server, since a pushState URL is a
      REAL url the host must answer (SPA fallback added, and the demo now reads
      its route from `location` because `history.state` is null on a fresh load).
      And `location` had only `href` and `hash` bound, so `location.pathname` was
      undefined and routing code calling `.split()` on it threw — the third
      instance this milestone of "the object exists, the members pages use do
      not", after `document.body` and `window.console`. Components added;
      assignment/`assign`/`replace`/`reload` deliberately left out as
      `T-JS-LOCATION-NAVIGATE-1`, because a setter that moves the reported URL
      without navigating is worse than an absent one.)*
- [x] 9.4.1: Declarative Request-Filtering Rules *(the big one. Named in the North
      Star, and it closes two extension-HOST holes as a side effect: `permissions`
      is parsed but not enforced, and rule sets need persistence the host lacks.
      A manifest field that is parsed and not enforced is worse than one that is
      absent — it reads as a boundary and is not one.)*
- [x] 9.4.2: Built-In Ad-Block-Lite Extension *(rides on 9.4.1. **Shipped
      deliberately thin**: manifest + `rules.json` + a `background.js` that only
      announces itself. A correct blocker needs no code, which is 9.4.1 working
      as designed rather than a shortcut — and it is stated in the file so nobody
      "fixes" it later. Required a second stub origin, `ads.example.net`: with
      only `example.dev` everything is first-party, which is exactly what a
      blocker must NOT block, so there was no `thirdParty` rule to exercise and
      no honest before/after to measure. The four acceptance counts come from one
      harness that loads the same page twice, blocker on and off, and diffs —
      with **bytes counted by the fake server**, since a request we never sent has
      no size we can know. `AdBlockLiteTest.TheShippedRuleSetIsValid` parses the
      **shipped** file, not a copy, and requires every rule to be third-party
      scoped; verified load-bearing by flipping the list to `firstParty` and
      watching it fail.)*
- [x] T-JS-MISSING-API-COVERAGE-1: widen the missing-API stub list *(the
      observable surface went from **2 names to 14**. Stubs for the observers
      (Intersection/Mutation/Resize/Performance), `customElements`, `WebSocket`,
      `requestIdleCallback`, `getComputedStyle`, `navigator`, `alert`/`confirm`/
      `prompt`, `structuredClone`. **`navigator` did not exist at all**, so
      `navigator.userAgent` — which a very large share of real pages read — was
      a `ReferenceError` that killed the entire script: the worst instance of
      the class this story exists to fix.
      **The rule the stubs follow is "never fabricate a value a page can branch
      on":** the socket reports `CLOSED` rather than pretending to be connected,
      `confirm()` returns false because no user agreed to anything,
      `getComputedStyle` returns `''` rather than a made-up length, and
      `navigator.userAgent` is empty rather than a plausible lie — M8 owns
      identity, and a second answer here would contradict it
      (`T-JS-NAVIGATOR-IDENTITY-1`).
      Two deliberate exceptions to "no-op": `requestIdleCallback` **does** run
      its callback on a timer, because pages defer real initialization into it
      and dropping it leaves the page half-built with no error to explain why;
      and `customElements.whenDefined` **never** resolves, because resolving
      would run post-upgrade code against an element that was never upgraded —
      a pending promise stalls one continuation, a false resolve corrupts
      everything after it.
      Tests assert each stub survives *realistic use* (construct, then call the
      methods a page calls), not merely that it exists — a stub that reports and
      dies on the next line is worse than none, since the page fails anyway and
      the telemetry claims it was handled. Verified load-bearing by making the
      socket claim `OPEN` and watching the honesty assertion fail. 964 green.
      Demo: a card on `example.dev/m9` that uses eight unimplemented APIs and
      reports how many calls completed.)*
- [x] T-HTML-ATTR-ENTITY-DECODE-1 + T-NET-DATA-URL-1 *(both P1 bugs found by the
      first live sweep, both fixed. Entity decoding ran on character data only,
      so `href="/wiki/Sam_&amp;_Max"` requested the entity and 404'd on a real
      article; reusing the text decoder is safe because it only accepts a
      SEMICOLON-terminated reference, which is exactly the spec's
      ambiguous-ampersand protection for attributes. `data:` URLs were handed to
      curl; a new `core/utils/DataUrl` parser answers them in the loader before
      any transport is chosen, covering images, stylesheets and fonts at one
      seam. Both verified load-bearing — the data-URL revert reproduces the
      original log line verbatim. Two further gaps filed rather than absorbed:
      `T-HTML-ENTITY-TABLE-1` (32 of ~2231 named entities; every accented Latin
      one missing, which biases against non-English pages) and
      `T-NET-RELOAD-FETCH-POLICY-1`.)*
- [x] T-JS-WINDOW-IS-GLOBAL-1: `window === globalThis` *(`window` was a separate
      `JS_NewObject` onto which a hand-picked subset of globals was mirrored, so
      `window.console`, `window.document`, `window.navigator`, `window.fetch` and
      every fail-soft stub were missing from it — and the list had to grow by hand
      forever. Now there is one object, as in a browser, so anything added later
      is reachable both ways for free.
      **Two more gaps had to close before the motivating page actually ran**,
      which is exactly why patching the symptom would have failed: an unqualified
      `addEventListener(...)` arrives with `this` **undefined** (quickjs does not
      substitute the global for a C function the way sloppy-mode JS does for a
      script function), so it was resolving to the DOCUMENT and its listener never
      saw a window event; and **`console` had exactly one method** — `log` — so
      `console.warn` was "not a function" and fixing `window.console` alone would
      have moved MediaWiki's death one line later. `console` now carries severity
      in the quickjs `magic` value, so a page's own `console.error` survives a
      build that only logs errors; previously everything went to INFO and the most
      important half of a page's diagnostics vanished first.
      All three verified load-bearing by reverting each in turn. The 9.0.2
      isolation tests stayed green, plus a new one asserting `window.x` does not
      survive into the next document — `window` is now the isolation surface, so
      that had to be pinned explicitly. 990 green.
      Demo: an m9 card checking ten identities, because a mirrored copy would pass
      a `typeof` check and still be the wrong object.)*
- [x] T-COOKIE-CONFORMANCE-VECTORS-1: cookie conformance number *(**36/36**,
      printed every CI run as `[cookie-conformance] N/M vectors passing`. Found a
      real gap on its first run: the `__Secure-`/`__Host-` name prefixes were not
      enforced at all, so a cookie could claim a guarantee it did not meet —
      fixed alongside. That is the argument for the whole exercise: prose said
      the module conformed, and the first number disagreed.)*
- [>] T-NET-IDENTITY-UI-1 (+ T-NET-IDENTITY-AUTOOFFER-1 behind it) — **moved to
      M11**. Chrome/UI work rather than fetch work; it was in M9 only because M9
      was the open milestone when it was filed. The gap is real and stays P1.
- [x] T-NET-RELOAD-FETCH-POLICY-1: a hard reload now reaches the data a page
      fetches while loading *(a script fetch always ran at CachePolicy::Default,
      so Ctrl+Shift+R refreshed the document and its subresources and then served
      the page's DATA from cache — on an API-driven page, the only thing the user
      came for. Scoped to a window that closes when the document's own scripts
      have run, because a reload has no business governing a fetch fired a minute
      later. Corrects a call recorded as deliberate during 9.3.2.)*
- [ ] 9.1.2: XHR Compatibility Wrapper *(**blocked, not scheduled** — its trigger
      is 9.5.2's count)*

Deferred out of M9 at the same re-triage — see the Carried Backlog table for the
reasoning: `T-HTML-PRESENTATIONAL-TAGS-1` → M10, `T-NET-CLIENT-HINTS-1` → M10,
`T-FONT-WOFF2-1` → M11, `T-STORAGE-DOT-ACCESS-1` → M12, and **9.2.2 Preflight
Cache → M12** (the kickoff probe already found Wikipedia sends no
`Access-Control-Max-Age`, so there is nothing to cache beyond the spec default;
it pays off against framework traffic, which is M12's subject).
