# Conformance: HTTP Cookies (RFC 6265 / 6265bis)

**Module:** `src/core/net/{Cookie,CookieJar}.{h,cpp}`, wired at
`src/engine/resources/ResourceLoader.cpp`.
**Status as of:** 2026-08-02 (M8 stories 8.1.0–8.1.2 + T-COOKIE-NAV-INITIATOR-1,
plus cookie name prefixes).
**Measured by:** `tests/fixtures/cookies/rfc6265_vectors.txt`, run by
`CookieConformanceTest` — currently **44/44 vectors passing**. The count is
printed on every CI run as `[cookie-conformance] N/M vectors passing` and may
only rise; a vector that fails must be marked `xfail` and name the ticket that
would fix it, so no failure sits here unexplained.

*The table earned its keep on its first run: it found that the `__Secure-` and
`__Host-` name prefixes were **not enforced at all**. That gap matters more than
its size suggests — the prefix is a promise the cookie's NAME makes to the
server (`__Host-session` means "set over https, by this exact host, for the whole
origin"), so accepting one that does not meet the conditions turns the guarantee
into a lie. A site can defend itself against an engine known to ignore prefixes;
it cannot defend against one that says yes and means no. Fixed in `CookieJar`
with a case-insensitive prefix match and a secure-origin requirement.*

*On the case question specifically — RFC 6265bis is **asymmetric** and is easy to
read the wrong way round. §4.1.3 tells **servers** to use a case-sensitive match
when producing a prefixed cookie; §5.4 tells **user agents** they "MUST match the
prefix string case-insensitively" when consuming one. We are a user agent, so
§5.4 binds, and it is a MUST. The reason is a concrete bypass
([httpwg/http-extensions#2231](https://github.com/httpwg/http-extensions/issues/2231),
closed by #2236): cookie names are case-sensitive, but many server frameworks
compare them case-insensitively, so an attacker-set insecure `__secure-session`
would be read as the protected `__Secure-session`. Matching case-insensitively
does not merge the cookies — `__Secure-a` and `__secure-a` remain distinct names,
which the vector table pins.*

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
| `Secure` | Yes | Rejected when received from a non-HTTPS origin and withheld from non-HTTPS transports. |
| `__Secure-` / `__Host-` prefixes | Yes | Case-insensitive prefix matching per §5.4; secure-origin and Secure-attribute checks, plus host-only and root-path checks for `__Host-`. |
| `HttpOnly` | Yes | Withheld from the script view (`script_visible_cookies`) while still riding HTTP requests. |
| SameSite `Lax` / `Strict` / `None` | Yes | Enforced for both subresources and navigations. `SameSite=None` without `Secure` is rejected at parse rather than downgraded. Lax default when absent or unrecognized. |
| 6265bis §5.4 None-requires-Secure | Yes | Rejected outright — a silent downgrade would look like it had worked. |
| §5.3 redirect handling | Yes | The engine owns the redirect loop (8.3.1), so cookies set mid-chain are stored before the next hop, and both the `Cookie` header and the SameSite context are recomputed per hop. |
| `document.cookie` | Yes | Read returns the script view (HttpOnly withheld); assignment sets one cookie through the same parser a server header uses, so attribute handling cannot drift between the two paths. |
| §5.3 persistence | Yes | Non-session cookies round-trip to a per-profile TSV file; session cookies are never written; expired ones are purged on load; a corrupt file starts empty. |

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
- **No size or count limits.** RFC 6265 §6.1 asks for at least 4096 bytes per
  cookie, 50 per domain, 3000 total. We enforce none, so a hostile server can
  grow the jar without bound.
  → `T-COOKIE-LIMITS-1` [M8 P2]
- **No name/value character validation.** Control characters and separators are
  stored as received rather than rejected, so a stored value could forge a second
  cookie when re-serialized into a `Cookie` header.
  → `T-COOKIE-CHARSET-1` [M8 P3]

### Functional gaps (scheduled work, not deviations we intend to keep)




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
