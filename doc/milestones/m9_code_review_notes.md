# Milestone 9 Code Review Notes

Review of `master...milestone/9-fetch` at `eb49bc5` (2026-08-02): 60 commits,
131 files changed, 15,100 insertions, and 418 deletions.

Release recommendation at review time: **hold the M9 release until every P0 and
P1 story below is complete.** P2 stories should either close before release or
be explicitly triaged into `doc/TODOs.md` with an owner and target milestone.

Verification baseline:

- `scripts/build.ps1`: passed incrementally (`ninja: no work to do`).
- `scripts/test.ps1`: 1,081 passed, zero failed, one skipped
  (`SmokeMainTest.StartsAndTicks`).
- `git diff --check master...HEAD`: failed on trailing whitespace in
  `doc/TODOs.md` lines 41 and 60.
- The review changed no production files.

Remediation completed through `10a15f8` (2026-08-02). The review fixes are split
into self-contained commits so final signoff can inspect or bisect each boundary:

- `cee8d45`, `7916a11`: History API origin guard and synchronous mutation queue.
- `4d6d11a`: explicit HTTP methods through the network port and adapters.
- `1f2ca66`: fetch credentials and forbidden response-header boundaries.
- `2023559`: secure-origin and exact-case cookie-prefix guarantees.
- `db97c6d`: shared preflight deadline and immutable filter initiator.
- `a6f2427`, `24a3114`: complete cache accounting and portability hygiene.
- `dd1833b`, `10a15f8`: final acceptance regressions and test formatting.

Current recommendation: **ready for final manual signoff.** Keep both milestone
status documents at Active until the one remaining unchecked gate—the post-fix
live `example.dev/m9` proof run—passes; then move both to Complete together.

## Release-blocking stories

- [x] **[M9 P0] T-M9-REVIEW-HISTORY-ORIGIN-1: Enforce pushState same-origin
      URLs**
  - **Goal:** a document must never acquire another origin's authority through
    `history.pushState` or `history.replaceState`.
  - **Cause:** `QuickJSScriptEngine::js_history_push_state` resolves arbitrary
    absolute URLs, and `Tab::process_script_history_change` adopts the result
    without navigation or a security-state reset. Cookies, local/session
    storage, fetch classification, and the URL bar subsequently use the adopted
    URL while the old document continues running.
  - **Scope:** validate the resolved URL against the active document origin
    before changing location, history, or Tab state; throw a `SecurityError` on
    mismatch. Leave all state unchanged on rejection.
  - **Acceptance:** a page at origin A cannot push or replace an origin-B URL;
    its URL bar, security state, cookies, storage, and subsequent fetch origin
    remain bound to A.
  - **Tests:** binding test for the exception and unchanged synchronous
    `location`; full-Tab regression proving a rejected cross-origin push cannot
    read origin-B cookie/storage or make a request classified as same-origin to
    B.

- [x] **[M9 P0] T-M9-REVIEW-NETWORK-METHOD-1: Carry the HTTP method through the
      network port**
  - **Goal:** OPTIONS, GET, HEAD, POST, PUT, DELETE, and other accepted fetch
    methods reach the server with the method the page requested, independently
    of body presence.
  - **Cause:** `INetwork` exposes only `get` and `post`; `ResourceLoader` selects
    between them solely from whether `post_body` exists. A preflight is therefore
    a GET carrying preflight headers, bodyless custom methods become GET, and any
    method with a body becomes POST. The CORS mock labels a GET as OPTIONS by
    inspecting its headers, masking the production behavior.
  - **Scope:** replace the method-specific port with an explicit request method
    (or add an equivalent general request operation), implement it in Curl and
    stub adapters, and preserve redirect method-rewrite semantics.
  - **Acceptance:** a real preflight is OPTIONS; an empty-body POST remains POST;
    DELETE/PUT/HEAD retain their method; GET never becomes POST merely because a
    body value exists.
  - **Tests:** transport-level assertions on the actual method, plus approved and
    rejected live-adapter preflight integration tests. Mocks must record the
    method passed through the port rather than infer it from headers.

- [x] **[M9 P0] T-M9-REVIEW-FETCH-SET-COOKIE-1: Never expose Set-Cookie to
      script**
  - **Goal:** `Set-Cookie` and `Set-Cookie2`, including HttpOnly session tokens,
    are never readable through a fetch `Response`.
  - **Cause:** forbidden-response-header filtering runs only for active
    cross-origin CORS requests. Same-origin responses are copied verbatim into
    the QuickJS response payload, and the same-origin test currently endorses
    exposing every header.
  - **Scope:** store response cookies in the jar first, then remove forbidden
    response headers from every script-visible response. Apply CORS exposure
    filtering as an additional cross-origin step, not as the only forbidden-
    header filter.
  - **Acceptance:** `response.headers.get("set-cookie")` is null for same-origin,
    cross-origin, cached, and 304-revalidated responses, while the cookie jar
    still processes live response cookies correctly.
  - **Tests:** same-origin HttpOnly regression, cross-origin regression, cached
    response regression, and 304 revalidation regression.

- [x] **[M9 P1] T-M9-REVIEW-FETCH-CREDENTIALS-1: Forward fetch credentials from
      the public wrapper**
  - **Goal:** `credentials: "omit"`, `"same-origin"`, and `"include"` reach the
    native binding unchanged.
  - **Cause:** the JavaScript `fetch()` prelude forwards only method, body, and
    headers even though the native binding already parses `credentials`.
  - **Acceptance:** `omit` suppresses same-origin cookies; `include` enables
    eligible cross-origin cookies and the corresponding stricter CORS verdict;
    the omitted option retains the existing same-origin default.
  - **Tests:** public-JavaScript binding tests that inspect the resulting
    `ScriptFetchRequest`, plus end-to-end cookie behavior through the wrapper.

- [x] **[M9 P1] T-M9-REVIEW-COOKIE-PREFIX-1: Complete secure cookie-prefix
      enforcement**
  - **Goal:** `__Secure-` and `__Host-` names provide the guarantees their names
    claim.
  - **Cause:** the new checks require a parsed `Secure` attribute but do not
    require a secure request URL, so an HTTP response can plant a prefixed cookie
    that is later sent over HTTPS. Prefix matching is also implemented
    case-insensitively despite cookie names and the prefix algorithm being
    case-sensitive.
  - **Scope:** reject prefixed cookies received from a non-secure origin, use the
    specified case-sensitive prefix match, and audit whether the same secure-
    origin rule must be applied to all `Secure` cookies at the common storage
    seam. Reference:
    <https://datatracker.ietf.org/doc/draft-ietf-httpbis-rfc6265bis/22/>.
  - **Acceptance:** HTTP cannot plant `__Secure-` or `__Host-`; the existing
    Secure/host-only/root-path rules remain enforced; differently cased ordinary
    cookie names retain ordinary cookie behavior.
  - **Tests:** add conformance vectors for insecure origin, exact-case prefixes,
    and differently cased non-prefix names.

- [x] **[M9 P1] T-M9-REVIEW-PREFLIGHT-DEADLINE-1: Give preflight and request one
      deadline**
  - **Goal:** the configured total timeout bounds the entire fetch, including
    preflight and the real request.
  - **Cause:** `send_preflight` receives an unstarted `RedirectChain` by value.
    `apply_deadline` starts a deadline only in the preflight copy; the approved
    real request then starts a second full budget.
  - **Scope:** establish the deadline before branching into preflight, and carry
    that same absolute deadline into the real request and every redirect hop.
  - **Acceptance:** a 15-second total budget cannot consume 15 seconds in
    preflight and another 15 seconds in the real request.
  - **Tests:** injected-clock test that spends part of the budget in preflight
    and observes only the remainder on the real request; exhausted preflight
    budget prevents the real request entirely.

- [x] **[M9 P1] T-M9-REVIEW-FILTER-REDIRECT-1: Keep filter attribution bound to
      the initiating document**
  - **Goal:** `thirdPartyOnly` filtering remains relative to the document across
    every redirect hop.
  - **Cause:** filtering reads `CookieRequestContext::initiator_host`, but redirect
    handling changes that value to the previous hop for SameSite cookie logic.
    A redirect within a tracker's registrable domain can therefore make the next
    tracker hop appear first-party.
  - **Scope:** add immutable document/filter initiator state to the redirect
    chain, separate from the mutable cookie request context.
  - **Acceptance:** a document-A request that redirects through tracker B to a
    same-site tracker C remains third-party at both B and C; SameSite cookie
    redirect behavior remains unchanged.
  - **Tests:** `thirdPartyOnly` redirect-chain regression, including two tracker
    hosts under one registrable domain and a first-party control case.

## Follow-up stories

- [x] **[M9 P2] T-M9-REVIEW-HISTORY-QUEUE-1: Preserve every synchronous history
      mutation**
  - **Goal:** multiple `pushState`/`replaceState` calls made before the Tab ticks
    have browser-equivalent history semantics.
  - **Cause:** the script engine stores one optional `HistoryChange`; each call
    overwrites the previous one before the Tab drains it.
  - **Acceptance:** two synchronous pushes add two entries in order; Back visits
    the intermediate entry; `history.length`, state, and relative URL chaining
    are correct. Replace operations affect the correct current entry.
  - **Tests:** binding queue test and full-Tab push/push/back and push/replace/back
    cases.

- [x] **[M9 P2] T-M9-REVIEW-CACHE-FOOTPRINT-1: Count the complete Vary key in
      cache limits**
  - **Goal:** per-entry and total cache byte caps reflect all owned dynamic
    memory that can be controlled through a request/response.
  - **Cause:** `HttpCache::Entry::footprint()` omits `vary_names` and copied
    `vary_values`; large selecting request headers can therefore evade the
    advertised caps.
  - **Acceptance:** a large `Vary` selecting value contributes to entry and total
    accounting, can trigger `TooLarge`, and participates correctly in eviction
    statistics.
  - **Tests:** oversized selecting-value rejection and total-cap eviction with
    several Vary variants.

- [x] **[M9 P2] T-M9-REVIEW-PORTABILITY-1: Close pre-PR include and whitespace
      failures**
  - **Goal:** the changed set passes the repository's portability checklist and
    `git diff --check` without relying on MSVC transitive includes.
  - **Scope:** add `<vector>` to `CacheControl.h`; add the direct `<mutex>` and
    `<cstdint>` dependencies to `Tab.h`; audit the rest of the M9 changed headers
    for every used `std::` name; remove the two trailing-whitespace lines from
    `doc/TODOs.md`.
  - **Acceptance:** the include audit is complete, `git diff --check
    master...HEAD` is clean, Windows builds/tests pass, and the changed set has
    been read for the GCC/libstdc++ portability incidents listed in the pre-PR
    checklist.

## Release-document decision

- [x] **[M9 P1 docs] Reconcile the roadmap, milestone status, and XHR scope**
  - `doc/milestones/roadmap.md` still labels M9 `Next` and promises “Fetch/XHR
    v1”; `doc/milestones/milestone9.md` remains `Active` and deliberately leaves
    9.1.2 XHR unscheduled.
  - **Recommended resolution:** explicitly defer XHR in the roadmap because the
    proof targets did not require it, describe M9 as Fetch v1 rather than
    Fetch/XHR v1, and move the roadmap and milestone statuses to `Complete`
    together only after the release-blocking checklist above and the documented
    manual proof gates pass.

## Final verification gate

- [x] Every P0 and P1 story above is complete with load-bearing regression tests.
- [x] Every P2 story is complete or moved to `doc/TODOs.md` in normal `T-*`
      format with an explicit target milestone.
- [x] `scripts/build.ps1` performs a real rebuild and passes (83 objects rebuilt
      during the changed-header audit; final test-format build rebuilt 8 objects).
- [x] `scripts/test.ps1` passes: 1,092 passed, zero failed, one understood GUI
      smoke skip (`SmokeMainTest.StartsAndTicks`).
- [x] `git diff --check master...HEAD` is clean.
- [x] `doc/dev_guide/pre_pr_checklist.md` is completed against the whole changed
      set, not only the files named by the first failure.
- [ ] The M9 manual live proof targets and demo page are re-run after the fixes.
- [x] Roadmap, milestone status, and release notes describe the same shipped
      scope.
