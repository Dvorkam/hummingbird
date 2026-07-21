# Conformance: HTTP Cookies (RFC 6265 / 6265bis)

**Module:** `src/core/net/{Cookie,CookieJar}.{h,cpp}`, wired at
`src/engine/resources/ResourceLoader.cpp`.
**Status as of:** 2026-07-21 (M8 stories 8.1.0–8.1.2 + T-COOKIE-NAV-INITIATOR-1).
**Measured by:** nothing yet — `T-COOKIE-CONFORMANCE-VECTORS-1` adds the vector
table that turns this page from a claim into a number.

Read `README.md` in this folder first: this file owns the *adherence picture*,
`doc/TODOs.md` owns the *work items*, and neither repeats the other.

## Why cookies are a good partial-conformance target

Most of the spec's substance — parse a `Set-Cookie`, decide whether it goes back
on a request — is a **pure function of strings**. That maps directly onto our
jar's API:

```
store_from_header(request_url, set_cookie_value, now)
cookie_header_for(request_url, now, context)  ->  "a=1; b=2"
```

So the bulk of the semantics can be tested by a data table with no browser, no
server, and no JS. That is unusual: it is why "a little, but real" is achievable
here and not, say, for layout.

The exception is **SameSite**, which is defined in terms of browsing context
(who initiated the request, top-level or not). It cannot be expressed as a
header→header vector and stays on targeted engine tests
(`tests/engine/ResourceLoader.test.cpp`).

## What we implement

| Spec area | Status | Notes |
|---|---|---|
| §4.1.1 `Set-Cookie` syntax | Yes | `name=value` plus Expires, Max-Age, Domain, Path, Secure, HttpOnly, SameSite. Unknown attributes ignored, per spec. |
| §5.1.1 cookie-date parsing | Yes | Delimiter-tokenized per the spec algorithm, not format-matched, so both `09 Jun 2021` and `09-Jun-2021` parse. Two-digit years map to 1970–2069. |
| §5.1.3 domain-matching | Partial | Host equality and subdomain suffix on a label boundary; IP literals match only themselves. **No public suffix check** — see deviations. |
| §5.1.4 path-matching + default-path | Yes | Including the `/app` vs `/application` boundary rule. Query and fragment are stripped before matching. |
| §5.2 parsing algorithm | Yes (subset) | Ignores a field with no `=` or an empty name. |
| §5.3 storage model | Yes | Host-only when no Domain; Max-Age overrides Expires; a Domain the request host is not within is rejected; same `(name, domain, path)` replaces in place, preserving creation time; an already-expired update deletes. |
| §5.4 retrieval + send order | Yes | Longer paths first, then earlier creation time. |
| `Secure` | Yes | Withheld from non-HTTPS transports. |
| `HttpOnly` | Yes | Withheld from the script view (`script_visible_cookies`) while still riding HTTP requests. |
| SameSite `Lax` / `Strict` / `None` | Yes | Enforced for both subresources and navigations. `SameSite=None` without `Secure` is rejected at parse rather than downgraded. Lax default when absent or unrecognized. |
| 6265bis §5.4 None-requires-Secure | Yes | Rejected outright — a silent downgrade would look like it had worked. |

## Deviations

Ordered by what they cost, not by spec order. Each links to its work item if one
exists; the ones marked *(no ticket)* are recorded but not scheduled.

### Security-relevant

- **No public suffix list.** `domain_matches` accepts `Domain=co.uk` from
  `example.co.uk`, which every other site under `co.uk` would then receive — a
  supercookie. The same list is needed for the same-site comparison, which today
  approximates the registrable domain as the last two labels and so treats
  `a.co.uk` and `b.co.uk` as same-site. libcurl gets this right by linking
  libpsl; an engine-owned jar must carry its own.
  → `T-COOKIE-PUBLIC-SUFFIX-1` [M8 P1]
- **No `__Secure-` / `__Host-` prefix enforcement.** Parsed like any other name;
  the prefixes' guarantees are not checked. Deliberate M8 non-goal — record here
  if a target site starts relying on them. *(no ticket)*
- **No size or count limits.** RFC 6265 §6.1 asks for at least 4096 bytes per
  cookie, 50 per domain, 3000 total. We enforce none, so a hostile server can
  grow the jar without bound. *(no ticket — worth one before untrusted browsing)*
- **No name/value character validation.** Control characters and separators are
  stored as received rather than rejected. *(no ticket)*

### Functional gaps (scheduled work, not deviations we intend to keep)

- **No redirect semantics.** libcurl still follows redirects internally
  (`CURLOPT_FOLLOWLOCATION`), so cookies set on intermediate hops are invisible
  and site context is not recomputed per hop. → M8 stories 8.3.1 then 8.1.3.
- **No persistence.** The jar dies with the process, so every cookie behaves as a
  session cookie across restarts. → M8 story 8.1.4.
- **No `document.cookie`.** The read filter exists (`script_visible_cookies`);
  nothing binds it to JS, and there is no write path. → M8 story 8.1.5.

### Environmental

- **No IDN / punycode handling.** Hosts are compared as ASCII, so an
  internationalized domain will not match its punycode form. *(no ticket — no
  proof target needs it)*
- **No third-party policy, partitioning, or cookie manager UI.** Explicit M8
  non-goals; a single shared jar per profile.
- **No encryption at rest.** Explicit M8 non-goal; the jar file will be plain
  text once 8.1.4 lands.

## If we ever go conformance-first

The cheap path is **not** WPT's `cookies/` suite, which is browser-driven and
asserts through `fetch`/XHR we stub until M9. Cheaper, in order:

1. **`T-COOKIE-CONFORMANCE-VECTORS-1`** — a pinned header→header vector table
   seeded from RFC 6265bis's normative examples and the `http-state` test
   vectors. No browser, no server. Covers everything in the table above except
   SameSite.
2. **The SameSite matrix stays bespoke** — it needs two real origins and a
   navigation context, which is what our engine tests already provide.
3. **WPT `cookies/` last**, once M9 gives us real `fetch` and a fixture server
   exists from story 8.4.1. At that point most of the harness cost is already
   paid.
