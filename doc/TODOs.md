# TODOs

Live backlog **index**. Planned stories live in the per-milestone docs under
`doc/milestones/`; completed work is archived under `doc/todo_archive/`. This file
holds the pointers plus the work items that no milestone doc owns yet.

Cross-cutting adherence registers (what conforms, what deviates, and why) live in
`doc/conformance/`: `rfc6265_cookies.md`, `html_tag_support.md`.

## Milestones 3-8 (shipped)

| Milestone | Stories | Archive |
|---|---|---|
| M3 Navigator | - | `doc/todo_archive/milestone3_done.md` |
| M4 Scripting | `doc/milestones/milestone4.md` | `doc/todo_archive/milestone4_done.md` |
| M5 Architect | `doc/milestones/milestone5.md` | `doc/todo_archive/milestone5_done.md` |
| M6 Layouter | `doc/milestones/milestone6.md` | - |
| M7 Programmable Document | `doc/milestones/milestone7.md` | - |
| M8 Session Keeper (+ M8.5) | `doc/milestones/milestone8.md` | `doc/todo_archive/milestone8_done.md` |

M8 shipped as `v0.8.0` on 2026-07-26. Its pre-merge review — one `rem` root-reference
bug fixed, one selector-validity gap filed — is recorded at the end of
`milestone8.md`.

## Milestone 9 (The Fetcher) - ACTIVE

Stories in `doc/milestones/milestone9.md`. Scope revalidated at kickoff 2026-07-26;
five findings changed the story list.

**Nine stories were still tagged `[M8]` after M8 shipped.** Five were genuine M9
prerequisites rather than leftovers, and are now stories:

- **`T-DISPATCH-MICROTASK-REENTRANT-1`** -> story **9.0.1** (foundational: fetch is
  promise-based, so microtask ordering becomes load-bearing for the first time).
- **`T-JS-GLOBAL-ISOLATION-1`** -> story **9.0.2** (foundational: 9.1.1's acceptance
  already requires clean per-document teardown for in-flight requests).
- **`T-COOKIE-PUBLIC-SUFFIX-1`**, **`T-COOKIE-LIMITS-1`**, **`T-COOKIE-CHARSET-1`**
  -> story **9.0.3**, one cookie-hardening slice (CORS credentials decisions lean on
  `is_same_site`, and fetch turns unbounded `Set-Cookie` into a page-controlled path).

Two more are carried as ordinary M9 backlog below (`T-NET-IDENTITY-UI-1`,
`T-COOKIE-CONFORMANCE-VECTORS-1`); two were re-homed (`T-FOCUS-MUTATION-SYNC-1` to
M11, `T-EVENT-SPEC-GAPS-1` to M12).

Four gaps found in the M8 pre-merge review became stories **9.1.3** (request
deadlines), **9.2.3** (CORS across redirect hops), **9.2.4** (response-header
exposure), and **9.3.2** (cache-key correctness: `Vary`/`private`/credentials). An
SPA-routing MVP was pulled forward from M12 as **9.6.1**.

### Carried M9 backlog (not on the North Star path)

**Re-triaged 2026-07-30**, once every M9 P0 had landed well ahead of schedule.
Four tickets were parked in M9 only because M9 was the open milestone; each has
been re-tagged above to the milestone that actually owns its layer
(`T-HTML-PRESENTATIONAL-TAGS-1` → M10, `T-NET-CLIENT-HINTS-1` → M10,
`T-FONT-WOFF2-1` → M11, `T-STORAGE-DOT-ACCESS-1` → M12), with the reason recorded
inline. The full recommendation, including the M9 stories themselves, is the
"Carried Backlog" table in `doc/milestones/milestone9.md`.

- [ ] **[M9 P1] T-NET-IDENTITY-UI-1: Proper Site-Settings UI For Compatibility Mode**; Goal: replace the Ctrl+Shift+U keyboard toggle with a discoverable, visible per-site control; Scope: a chrome affordance (site-info button / menu entry / page-action) that shows the current site's identity mode and lets the user switch it, plus ideally a one-line banner when a site is in Compatibility mode so it is never a hidden state; reuse `IdentityPolicyStore` (the model is done). Consider surfacing *why* (the site rejected the honest UA) when the Phase-2 auto-detect (T-NET-IDENTITY-AUTOOFFER-1) is present; Acceptance: a user can see and change a site's identity mode without knowing a keyboard shortcut; Tests: chrome interaction tests. *(Filed 2026-07-22 at the user's request: Phase 1 shipped the keyboard toggle to reach the HN North Star fast; this is the real user-facing surface. Depends only on T-NET-IDENTITY-1.)*

- [x] **[M9 P1, DONE 2026-07-30] T-DOM-DOCUMENT-BODY-1: `document.body`, `document.head`, `document.documentElement`**; Goal: give scripts the three document entry points every page assumes exist; Scope: `install_document_bindings` (`QuickJSScriptEngine.cpp`) exposes `getElementById`, `createElement`, the query methods and the EventTarget methods, but **none of the three standard element accessors**. `document.body` is `undefined`, so the single most common append idiom in existence — `document.body.appendChild(el)` — throws `TypeError: cannot read property 'appendChild' of undefined`. Add read accessors resolving to the `<body>`, `<head>` and root `<html>` elements of the current document (the DOM already has them; this is a binding gap, not a tree gap), wrapped through the same node-wrapper path `getElementById` uses so identity and prototypes match. `document.body` is also writable per spec — out of scope, reading is what pages actually do; Acceptance: `document.body.appendChild(document.createElement('p'))` renders the new element, and `document.documentElement.tagName === 'HTML'`; Tests: script-binding tests for each accessor plus one appendChild-to-body render case. *(Filed 2026-07-30 while writing the 9.5.1 harness: the cross-origin fixture page appended its result to `document.body` and the whole flow failed with a `TypeError` that the page's own `.catch` then reported as a network failure — which is exactly how this presents in the wild, as an unrelated-looking error. The fixture was pointed at a pre-existing element to keep the story focused. **Worth checking against missing-API telemetry before M12 scoping** — an undefined property is not a missing *call*, so this gap may be invisible to the counters that are supposed to find gaps like it.)*

- [ ] **[M10 P2] T-HTML-TREE-SKELETON-1: Synthesize The `html`/`head`/`body` Skeleton During Parsing**; Goal: give every parsed document the element skeleton the HTML spec's tree construction guarantees, instead of only the tags the author happened to write; Scope: `HtmlParser` builds a synthetic `root` wrapper and then appends whatever tags appear, so `<p>hi</p>` yields `root > p` with **no `html`, `head` or `body` element at all** — where every real browser yields `html > head + body > p`. Implement the insertion-mode part of tree construction that creates the three implied elements (and routes `<title>`/`<meta>`/`<link>`/`<style>` into head, everything else into body); Acceptance: a document with no `<html>`/`<body>` tags still exposes `document.body`, and a `body { }` CSS rule matches it; Tests: parser tree-shape tests for bare fragments, implied-tag documents, and misnested skeletons. *(Filed 2026-07-30 during `T-DOM-DOCUMENT-BODY-1`, which needed to know what `document.body` should return. It returns **null** for such a document — deliberately honest, since substituting the synthetic root would make `document.body.tagName === 'ROOT'` and quietly mislead every page that checks. **This is the deeper half of the same problem** and is why the binding alone does not close the whole gap; it also means UA/author `body`/`html` selectors silently match nothing on those documents. Sized for M10, which owns the style/layout consequences of the skeleton.)*

- [ ] **[M9 P2] T-NET-RELOAD-FETCH-POLICY-1: A Hard Reload Does Not Reach A `fetch()` Fired During Page Load**; Goal: make Ctrl+Shift+R refresh the data an API-driven page loads with, not just its document and subresources; Scope: `ResourceLoader::fetch_for_script` builds its `RedirectChain` without ever setting `chain.cache_policy`, so a script fetch always runs at `CachePolicy::Default`. Subresources inherit the navigation's policy through `nav_cache_policy_`; script fetches do not. The result is that a page whose inline script fetches its data on parse — the shape of every SPA, and of M9's own proof target — serves that data from cache on a hard reload, which is the one gesture that is supposed to guarantee freshness. Thread the navigation's cache policy to script fetches issued **while the document is still loading**, which needs a "navigation in progress" signal the loader does not currently expose; a fetch fired later must keep the Default policy, since a reload legitimately does not govern a request made a minute afterwards; Acceptance: a hard reload on a page that fetches during load re-requests that URL from the network; a fetch fired after load still uses the cache; Tests: an `ApiRenderFlow`-style case asserting the API is re-requested after `hard_reload()`, plus one asserting a post-load fetch is not. *(Filed 2026-07-30. **Correcting an earlier call of mine:** during 9.3.2 this was recorded as deliberate on the strength of the m9 demo card's "not for a `fetch()` you fire a minute later", which is true and correct for a LATER fetch — but it never addressed the during-load case, and browsers do reach that one. The deliberate half and the gap half were conflated. Narrow, but it makes the cache un-escapable for exactly the page shape M9 exists to serve.)*

- [x] **[M9 P2, DONE 2026-07-30] T-JS-MISSING-API-COVERAGE-1: Widen The Missing-API Stub List So The Telemetry Can Actually Inform M12**; Goal: make the missing-API counters observe enough of the JS surface to be worth deriving a milestone's scope from; Scope: the fail-soft prelude in `QuickJSScriptEngine::install_failsoft_stubs` reports exactly four names — `XMLHttpRequest`, `matchMedia`, `localStorage`, `sessionStorage` — and the last two have been **implemented since 8.2.2/8.2.3**, so their stubs never fire. Two names is the entire observable surface. Add reporting stubs for the APIs a real page is actually likely to reach for (`IntersectionObserver`, `MutationObserver`, `ResizeObserver`, `customElements`, `WebSocket`, `requestIdleCallback`, `getComputedStyle`, `structuredClone`, `navigator.clipboard`, `IndexedDB`, `Notification`, `ServiceWorker`), each `typeof`-guarded and no-op like the existing ones so a real implementation later wins; **and decide what to do about missing properties**, which this mechanism cannot see at all (see below); Acceptance: a page touching any listed API produces a `[missing-api]` line, and the list is reviewed at M12 kickoff rather than assumed; Tests: prelude tests asserting each stub reports once and does not throw. *(Filed 2026-07-30 by story 9.5.2. **Why this matters more than it looks:** the roadmap says M12's scope comes from this telemetry at kickoff. An instrument that sees two names cannot carry that. It also quietly biases 9.1.2 — XHR's pull-in trigger is "telemetry reports it", and XHR is one of only two things the telemetry CAN report, so a hit would measure visibility rather than demand. **Known structural limit to state in the story, not silently work around:** the prelude stubs GLOBALS THAT GET CALLED, so a missing *property* — `document.body` was exactly this, per `T-DOM-DOCUMENT-BODY-1` — reads as `undefined` and throws at the use site with nothing reported. Closing that needs a different mechanism (an exotic `get_own_property` on `document`/`window` that reports unknown reads), which is a bigger and riskier change; scope this story to the globals and file the property half separately if it is wanted.)* **DONE 2026-07-30:** the surface went from 2 observable names to 14. Stubs added for `IntersectionObserver`, `MutationObserver`, `ResizeObserver`, `PerformanceObserver`, `customElements`, `WebSocket`, `requestIdleCallback`, `getComputedStyle`, `navigator`, `alert`/`confirm`/`prompt`, `structuredClone`. **`navigator` did not exist at all**, so `navigator.userAgent` was a ReferenceError that killed the whole script — the single worst instance of the class. Two judgment calls worth keeping: stubs never fabricate a value a page can branch on wrongly (the socket reports CLOSED, `confirm()` returns false, `getComputedStyle` returns empty, `navigator.userAgent` is `''`), and `requestIdleCallback` **does** run its callback on a timer, because pages defer real initialization into it and dropping it leaves the page half-built with no error to explain why. `customElements.whenDefined` deliberately never resolves: resolving would run post-upgrade code against an element that was never upgraded. Property-level telemetry remains out of scope and unsolved — filed as part of `T-DOM-DOCUMENT-BODY-1`'s note; `navigator.userAgent`'s real value is `T-JS-NAVIGATOR-IDENTITY-1`.

- [ ] **[M11 P2] T-JS-NAVIGATOR-IDENTITY-1: Answer `navigator.userAgent` From The Real Identity Policy**; Goal: make the JS-visible identity agree with the one the network layer sends; Scope: `T-JS-MISSING-API-COVERAGE-1` added a `navigator` stub whose `userAgent` is deliberately the **empty string**, because this engine has a considered per-origin identity policy (M8's Transparent/Compatibility modes, `IdentityPolicyStore`, toggled with Ctrl+Shift+U) and a JS surface answering something different from the `User-Agent` header would be lying in two directions at once. Thread the effective identity for the current document's origin through `IScriptHost` and answer `navigator.userAgent` (and `platform`, `appVersion`, the `userAgentData` low-entropy hints) from it, so flipping a site to Compatibility mode changes what script sees exactly as it changes what the server sees; Acceptance: `navigator.userAgent` equals the `User-Agent` this origin is sent, and follows the per-site toggle; Tests: binding tests per identity mode. *(Filed 2026-07-30. The empty string is safe but not free — a page sniffing the UA takes its unknown-browser path. Grouped with M11 because that milestone already touches the user-facing identity surface via `T-NET-IDENTITY-UI-1`; pull it forward if a proof target sniffs.)*

- [ ] **[M12 P2] T-JS-GET-COMPUTED-STYLE-1: Real `getComputedStyle`**; Goal: let script read the styles the engine actually computed; Scope: `T-JS-MISSING-API-COVERAGE-1` added a stub that reports and returns `''` for every property — deliberately, because a stub inventing plausible lengths would be worse than an empty one: layout-reading code would act on the numbers. The engine HAS the data (`ComputedStyle` per node), so this is an exposure story: a `CSSStyleDeclaration`-shaped read-only view over the computed style, resolved values where cheap (used value for lengths after layout, per spec), served through `IScriptHost`; Acceptance: `getComputedStyle(el).getPropertyValue('color')` returns the computed colour, and a length reflects layout; Tests: binding tests per property class. *(Filed 2026-07-30. M12 because frameworks and measurement code are its subject, and because resolved-value semantics depend on layout being settled — which M10 is still changing.)*

- [ ] **[M9 P2] T-COOKIE-CONFORMANCE-VECTORS-1: A Cheap Cookie Conformance Number**; Goal: a tracked adherence figure for the cookie module without adopting WPT wholesale; Scope: a pinned fixture table of `(Set-Cookie header(s), request URL) -> expected Cookie header` under `tests/fixtures/cookies/`, driven by one data-driven gtest reporting `N/M passing`, seeded from RFC 6265bis's normative examples and the `http-state` vectors; Acceptance: CI prints a pass count that may only rise, and each known-failing vector names the ticket that would fix it; Tests: the harness is the test. *(Rationale and what it can/cannot cover: `doc/conformance/rfc6265_cookies.md`. Do this AFTER 8.3.1/8.1.3 so redirect behavior is settled before the number is pinned.)*

- [ ] **[M12 P2] T-STORAGE-DOT-ACCESS-1: Property Access On localStorage (`localStorage.foo` / `localStorage['foo']`)**; Goal: support the dot/bracket idiom pages use alongside getItem/setItem; Scope: give the localStorage object exotic `get_own_property`/`set_property`/`has`/`delete` handlers that route unknown keys to the storage methods while letting the real methods and `length` resolve normally (the DOMStringMap dataset exotic is the near template, but it fully overrides get; localStorage must fall through to its methods, so `get_own_property` + prototype methods is the correct shape, or a prelude Proxy if this build enables Proxy); Acceptance: `localStorage.foo = 'x'; localStorage.foo` and `localStorage['foo']` round-trip, and `for (var k in localStorage)` enumerates keys; Tests: binding tests for dot, bracket, delete, and enumeration. *(Deferred from 8.2.2, which shipped the spec-required methods + length + QuotaExceededError. Dot/bracket access is a common convenience but needs the exotic proto-fallthrough done carefully; kept out of the core story to avoid risking it. Assess Proxy support first — it is the simplest correct implementation if available. **Re-homed M9 → M12 at the 2026-07-30 re-triage:** a convenience idiom rather than a correctness hole, and M12 owns the JS surface pages assume. Pairs with `T-DOM-DOCUMENT-BODY-1`, which is the same class of gap but IS a correctness hole, and stays in M9.)*

- [ ] **[M9 P2] T-NET-IDENTITY-AUTOOFFER-1: Offer Compatibility Mode When A Safe Top-Level Request Is Rejected**; Goal: proactively suggest Compatibility mode when a site rejects Hummingbird's honest identity, instead of the user having to know about the toggle; Scope: when a **top-level, safe (GET)** navigation to a Transparent-mode origin returns 429/403 with the shape of an identity rejection, set a flag on the document result and have the chrome show a non-modal bar — "This site rejected Hummingbird's identity. Retry in compatibility mode?" — that on accept sets the origin to Compatibility and re-navigates. **Never automatic, never for a POST.** Note the HN wrinkle: HN accepts the GET and only rejects the *POST*, so this path will not trigger for HN — it is for sites that reject the initial navigation; the manual control (T-NET-IDENTITY-UI-1) remains the primary mechanism; Acceptance: a site that 429s the top-level GET produces the offer bar, and accepting it loads the page; a POST rejection never auto-offers; Tests: engine flag + chrome bar tests. *(Filed 2026-07-22 as Phase 2 of the identity work. Depends on T-NET-IDENTITY-1.)*

- [ ] **[M10 P2] T-HTML-PRESENTATIONAL-TAGS-1: Legacy Presentational Tags (`<center>`, `<u>`, …)**; Goal: give legacy presentational elements their intended box and decoration instead of rendering them as generic unstyled blocks; Scope: `<center>` and `<u>` warn as "Unsupported HTML Tag" and fall through to the generic `display:block` default — so `<center>` does not center (HN wraps its whole `#hnmain` in it, so the page renders left-hugging) and `<u>`, being laid out as a block rather than inline, both loses its underline **and breaks the surrounding inline text flow**. Add them to `kKnownTags` (`HtmlTagMetadata.h`) and give them UA defaults in `StyleDefaults::apply_user_agent_defaults` — `<center>` → `display:block; text-align:center`, `<u>` → `display:inline` + underline (mirror the `<a>` branch's `underline` set) — routed through the same `ComputedStyle` fields real CSS uses so a page's own CSS still overrides them; the sibling legacy tags (`<s>`/`<strike>`/`<small>`/`<big>`/`<tt>`/`<sub>`/`<sup>`/`<mark>`) are the same shape and can ride along when a target needs them; Acceptance: HN's top-level content centers and any inline `<u>` renders inline+underlined without breaking its line; Tests: style-default tests asserting the display/text-align/underline for each, plus a layout check that `<u>` stays inline. *(Filed 2026-07-22 from the HN sidetour. Small and contained; the `<center>` half is the more visible one for HN. See the register for the full tier model and the CSS-property gaps `resize`/`word-break`/`overflow-wrap`/`page-break-before`, which are cosmetic-only on that page and not scheduled. **Re-homed M9 → M10 at the 2026-07-30 re-triage:** these are UA style defaults plus an inline-flow bug, and M10 owns both the inline box model and the UA defaults.)*

- [ ] **[M11 P2] T-FONT-WOFF2-1: WOFF/WOFF2 Web Font Decoding**; Goal: decode WOFF and WOFF2 web fonts so real icon/text web fonts render (DDG's `ddg-serp-icons` magnifier glyph is WOFF2); Scope: add a WOFF2 decoder (Google `woff2` + `brotli` — note `brotlidec`/`brotlicommon` are already pulled in by libcurl) and a WOFF (zlib) path, converting to raw SFNT bytes before handing to Blend2D; wire it into the font resolver's loadability check (`is_loadable_font_format`) so `format(woff2)`/`.woff2` sources stop being skipped, and load the decoded bytes via the on-disk font cache (or `createFromData`); Acceptance: an `@font-face` whose only src is a `.woff2` renders its glyphs instead of falling back; Tests: font decode unit tests + resolver integration. *(T-FONT-FACE-1 shipped @font-face parsing, family→src resolution, style-time `font_src`, and local + remote **raw TTF/OTF** loading; WOFF2 was deferred because Blend2D decodes only raw SFNT and no WOFF2 decompressor is bundled. The resolver already picks a raw-TTF `src` over a WOFF2 one when both are offered. **Re-homed M9 → M11 at the 2026-07-30 re-triage:** §11.4 is the text-rendering milestone, and this vendors a new dependency (`woff2` + `brotli`) that belongs with the rest of the font stack rather than landing alone.)*

- [ ] **[M10 P3] T-NET-CLIENT-HINTS-1: Honor `Accept-CH` Before Sending High-Entropy Client Hints**; Goal: send high-entropy UA client hints only to origins that ask for them, per the Client Hints spec; Scope: parse `Accept-CH` from responses, remember per-origin which hints an origin requested, and only then emit `Sec-CH-UA-Full-Version-List` (`"Hummingbird";v="0.2"`) and any other high-entropy hints; until then keep sending only the three low-entropy hints (`Sec-CH-UA`, `-Mobile`, `-Platform`) that ship today; Acceptance: a fresh origin gets only low-entropy hints; after it sends `Accept-CH: Sec-CH-UA-Full-Version-List`, the next request includes it; Tests: client-hints negotiation tests. *(Filed 2026-07-22 alongside T-NET-IDENTITY-1, which sends the low-entropy hints unconditionally and withholds the full-version list. **Re-homed M9 → M10 at the 2026-07-30 re-triage:** it pairs with `T-NET-EFFECTIVE-REQUEST-HEADERS-1`, already M10 P3 — both ask whether the cache key matches the headers actually sent, and splitting them across milestones is how one gets fixed and the other forgotten. The original M9 coupling ("land it after 9.3.2") is satisfied either way now that 9.3.2 has shipped.)*

- [ ] **[M10 P2] T-NET-CACHE-PARTITION-1: Partition the HTTP Cache by Top-Level Site**; Goal: stop one site being able to learn what another site's resources cost to load; Scope: real browsers added double-keyed caching (~2020) because a shared cache is a cross-site oracle — a site can time a subresource fetch and infer whether you have visited somewhere else. Add the top-level site to the cache key alongside method+URL+`Vary`+credentials class (`core/net/HttpCache.h`), which means the loader must thread the initiating document's top-level site down to `send_request`; note this deliberately costs hit rate on genuinely shared assets, which is the trade browsers already made; Acceptance: two different top-level sites requesting the same URL do not share an entry; Tests: cache-key tests per top-level site. *(Filed 2026-07-29 during 9.3.2. This gap applies to every entry, not only `Cache-Control: private` ones, which is why 9.3.2 stores `private` rather than half-mitigating the real problem by declining one directive.)*

- [ ] **[M10 P3] T-NET-EFFECTIVE-REQUEST-HEADERS-1: Key the Cache on the Headers Actually Sent**; Goal: make `Vary` correct for headers the transport owns rather than the engine; Scope: `INetwork` backends add some request headers themselves — notably `Accept-Encoding`, which libcurl sets — so a response that says `Vary: accept-encoding` is keyed on the engine's empty value. That is *consistent* (the transport's value is fixed for a build) but wrong in principle, and it would conflate variants the day it stops being fixed. Either have the engine own `Accept-Encoding` explicitly, or have `NetworkResponse` report the effective request headers so the cache can key on them; Acceptance: a response varying on a transport-set header is keyed on the value that was really sent; Tests: a cache-key test using a transport-owned header. *(Filed 2026-07-29 during 9.3.2, where HNPWA's real `Vary: x-fh-requested-host, accept-encoding` made the gap concrete. Couples to `T-NET-CLIENT-HINTS-1`, which adds more request headers responses may vary on.)*

## Milestone 10 (The Layouter II) - planned

Stories in `doc/milestones/milestone10.md`, created 2026-07-26 from this backlog.

Moved there: `T-CSS-ABS-STATIC-1`, `T-CSS-INSET-SHORTHAND-1`,
`T-LAYOUT-INTRINSIC-SIZE-CONSTRAINT-1`, `T-CSS-PERCENT-HEIGHT-PROPAGATE-1`,
`T-CSS-REPLACED-PERCENT-INLINE-1`, `T-CSS-ASPECT-RATIO-1`,
`T-CSS-SELECTOR-UNSUPPORTED-DROP-1`, `T-CSS-PAREN-TOKENS-1`,
`T-CSS-LENGTH-RESOLVER-1`, `T-CSS-DECL-DISPATCH-1`, `T-CSS-FLEX-ALIGNMENT-2`,
`T-CSS-GRID-TEMPLATE-AREAS-1`, `T-LAYOUT-FLEX-WRAP-COL-1`,
`T-LAYOUT-FLEX-BASELINE-2`, `T-CSS-INLINE-SPACING-1`, `T-CSS-TABLE-BORDER-MODEL-1`,
plus nine tech-debt items grouped as section 10.8 (`T-DOM-STYLE-COUPLING-1`,
`T-URL-POLYFILL-DIVERGENCE-1`, `T-HTML-ESCAPE-DEDUP-1`,
`T-CHECKBOX-DETECT-DEDUP-1`, `T-QUERYSELECTOR-CACHE-1`, `T-BOOKMARK-STORE-IO-1`,
`T-QUICKJS-BINDING-DEDUP-1`, `T-EXT-MANIFEST-FIELD-HELPER-1`,
`T-INVALIDATION-BUDGET-CLICK-DOUBLE-PASS-1`).

Three new stories were written for the roadmap's own headline items, which had never
been ticketed: `position: fixed`/`sticky` (10.1.1), real scroll containers (10.2.1),
and stacking contexts / `z-index` (10.3.1).

**Added at the 2026-07-30 M9 re-triage:** `T-HTML-PRESENTATIONAL-TAGS-1` (UA style
defaults plus an inline-flow bug — M10 owns the inline box model) and
`T-NET-CLIENT-HINTS-1` (pairs with `T-NET-EFFECTIVE-REQUEST-HEADERS-1`, already
here). Both still carry their full ticket text in the M9 backlog list above; they
need story bodies written into `milestone10.md` at that milestone's kickoff.

## Milestone 11 (The Inputter) - planned

Stories in `doc/milestones/milestone11.md`, created 2026-07-26 from this backlog.

Moved there: `T-FORM-INPUT-PASTE-1` and `T-FORM-PASSWORD-MASK-1` (both were tagged
`[M9]`; re-homed to the clipboard/forms milestone that owns them, though both are
small enough to pull forward if a demo needs them), `T-FORM-TEXTAREA-EDITING-1`,
`T-FORM-TEXTAREA-API-1`, `T-FORM-TEXTAREA-LAYOUT-1`, `T-FORM-SELECT-1`,
`T-FORM-CONTROL-CSS-1`, `T-FORM-INPUT-WIDTH-1`, `T-FORM-FOCUS-UA-OUTLINE-1`,
`T-CSS-TEXT-WRAP-2`, and `T-FOCUS-MUTATION-SYNC-1` (was tagged `[M8]`; it is a
focus-system bug, so it belongs with the focus system).

**Added at the 2026-07-30 M9 re-triage:** `T-FONT-WOFF2-1` — §11.4 is the
text-rendering milestone, and the story vendors a new dependency (`woff2` +
`brotli`) that belongs with the rest of the font stack. Needs a story body written
into `milestone11.md` at kickoff.

**Note:** `T-FORM-INPUT-PASTE-1` and `T-FORM-PASSWORD-MASK-1` are already full
stories here (**11.1.1** and **11.1.2**). `milestone9.md`'s carried-backlog table
still listed them as M9 P1 until 2026-07-30 — an index that is not regenerated from
its source drifts from it, which is the same failure the M9 kickoff found at
milestone scale.

## Milestone 12 (The Framework Gauntlet) - aspirational

Draft in `doc/milestones/milestone12.md`, created 2026-07-26. Deliberately thinner
than M9-M11: M12's scope is meant to be derived from missing-API telemetry at
kickoff, not guessed in advance.

Moved there: `T-DOM-CUSTOM-1`, `T-DOM-1`, `T-CSS-MULTICOL-1`,
`T-CSS-VISUAL-EFFECTS-1`, and `T-EVENT-SPEC-GAPS-1` (was tagged `[M8]`; its open
items — `window` on the propagation path, `CustomEvent` constructors, listener
options — are framework surface).

**Added at the 2026-07-30 M9 re-triage:** `T-STORAGE-DOT-ACCESS-1` (a convenience
idiom needing exotic-object handlers — M12 owns the JS surface pages assume) and
**M9 story 9.2.2 Preflight Cache** (the kickoff probe found Wikipedia sends no
`Access-Control-Max-Age`, so it pays off against framework traffic, not against
M9's proof targets; the story body is in `milestone9.md` §9.2 and should move here
at M12 kickoff). Both need story bodies in `milestone12.md`.

## Not yet owned by a milestone doc (M13+)

No milestone doc exists for M13-M16 yet, so these keep their full text here. Write
the M13/M14 docs once M12 is underway and the perf/compositor picture is concrete.

- [ ] **[M13 P1] T-PERF-LAYOUT-INCREMENTAL-1: Layout Is Recomputed Whole On Every Interaction**; Goal: stop a single interaction (click, focus, hover, inspect) on a large page from re-running the entire style+layout pipeline from scratch; Scope: on the Hacker News item page (`item?id=...`, ~1 MB, ~32k DOM nodes, one big comment-tree table, 671 comments) a full document rebuild measures **`layout ms ≈ 1900`** while style is ≈96 ms and render-tree build ≈10 ms — layout dominates at ~58 µs/node — and the telemetry shows that whole ≈1.9 s pass repeating on *each* click, because there is no dirty-tracking or cached layout: every input task rebuilds the full render tree and re-lays-out the entire document. Two sub-problems, likely staged: **(a)** a hot path that re-lays-out an unchanged tree should be a cheap no-op / cache hit, not a rebuild — introduce layout invalidation so only dirty subtrees re-run (this is the interactivity killer, and the bigger win); **(b)** the *initial* ≈1.9 s cost of laying out this page at all — profile first to confirm the hotspot is the giant `border=0` comment table's auto-layout (nested/deep table measurement is the prime suspect, cf. the intrinsic-width work in 7.4.2 / T-LAYOUT-TABLE-INTRINSIC-BLOCK-1) before deciding between targeted table-layout optimization and a broader measurement cache; Acceptance: a click that changes nothing (or only a small subtree) on the HN item page does **not** trigger a full-document relayout — the per-interaction pass drops by orders of magnitude — and the initial layout of that page is profiled with the dominant cost named; Tests: an invalidation test (interaction on a large fixture advances the layout-pass work counter by a bounded amount, not a full rebuild), plus a layout-pass count assertion in the spirit of 7.4.1's one-pass-per-task test. *(Filed 2026-07-22 from the HN sidetour — the first real page whose size makes the whole-document relayout model visibly janky (~2 s per interaction, "spins the fan"). Distinct from the M14 raster/compositor items below, which cache *paint*; this caches *layout*. Not a North-Star blocker (login+comment+persist works regardless of smoothness), but it is the most user-visible perf gap found on a real page, so P1 within its milestone and a pull-forward candidate if M9+ real-web browsing makes it worse. Related: T-PERF-4, T-QUERYSELECTOR-CACHE-1.)*

- [ ] **[M14 P1] T-PERF-4: Offscreen Raster Cache + Layer Invalidation**; Goal: repaint only dirty regions instead of the whole frame; Scope: renderer + engine invalidation (cache rasterized layers, invalidate on change); Acceptance: cached layers reused across frames; Tests: renderer perf tests. *(Moved from M6 to M14 The Compositor: this is compositor/retained-rendering work, which M6's own non-goals explicitly exclude; the roadmap puts raster strategy + perf-budget CI assertions at M14.)*

- [ ] **[M14 P2] T-CACHE-1: Tab Resource Eviction + Rehydrate**; Goal: evict resources/render tree for background tabs and restore on focus; Scope: Tab/TabManager + ResourceStore; Acceptance: inactive tabs drop memory and reload on activation; Tests: engine tests. *(Moved from M6: tab-memory architecture, not forced by any current page. Grouped with the M14 perf/retained-rendering track; pull forward to a dedicated perf pass if multi-tab memory becomes a real problem sooner.)*

- [ ] **[M15 P2] T-PROFILE-DATA-DIR-1: Move Profile Data To A Per-User Directory**; Goal: stop writing cookies/bookmarks/storage in the clear beside the executable; Scope: resolve a per-user, OS-permissioned profile dir (`%LOCALAPPDATA%\Hummingbird` on Windows, `$XDG_CONFIG_HOME`/`~/.config/hummingbird` on Linux) and route `CookieJar`/`BookmarkStore`/storage default paths there instead of `assets/config/`; keep the `HB_*_FILE` overrides for tests; Acceptance: a fresh run creates its files under the profile dir, and nothing sensitive is written into the build/asset tree; Tests: path-resolution unit tests per platform. *(Location is the cheap, high-value half of at-rest safety: the OS already permissions a per-user dir to that user, which is the real boundary. Encryption at rest — T-STORAGE-ENCRYPTION-1 territory — is shallow even in shipping browsers, so this matters more. Recorded 2026-07-21 answering "why is the TSV file sitting in the program folder?".)*

- [ ] **[M15 P3] T-COOKIE-DURABILITY-1: Persist Cookies Incrementally Instead Of Only At Shutdown**; Goal: stop a crash or a kill discarding every cookie acquired since startup; Scope: write the jar on a debounced timer and/or when a persistent cookie is stored, replacing the single `BrowserApp::shutdown()` write; use a write-to-temp-then-rename so a crash mid-write cannot corrupt the file (the current `trunc` write can); Acceptance: killing the process retains cookies stored more than the debounce interval earlier, and an interrupted write never leaves an unreadable file; Tests: jar durability tests + a simulated interrupted write. **Evaluate SQLite here rather than in a separate story:** the only compelling reason to adopt a DB for cookies/storage is atomic crash-safe incremental writes, which is exactly this story — the indexing/query wins are marginal at our scale (dozens of cookies, a handful of origins). If temp-then-rename over TSV proves fiddly, SQLite/WAL delivers the same guarantee for the cost of a dependency and a schema. Weigh that against the pedagogical value of a `cat`-able plaintext file, which is real for an educational engine. Decide at implementation time; do not pull the dependency in speculatively. *(Accepted limitation for M8: the jar is written once, in shutdown, so `kill -9` loses the session. Deliberately deferred — the North Star only needs a clean close-and-reopen, and real durability belongs with the wider profile-data hardening in M15.)*

- [ ] **[M15 P3] T-STORAGE-EVICTION-1: Reclaim Stale localStorage Across Origins**; Goal: stop indefinitely-retained per-origin storage from silently accumulating into a large, mostly-unused disk footprint; Scope: track a last-accessed time per origin store, enforce a total (all-origins) budget on top of the per-origin quota, and evict least-recently-used origins when it is exceeded; optionally surface a review affordance (per-origin sizes + last use, with a clear action) rather than evicting silently; Acceptance: total storage stays under the global budget by dropping the least-recently-used origins first, and an origin in active use is never evicted; Tests: multi-origin eviction-order tests. *(Recorded 2026-07-22, user's observation: unlike cookies, localStorage has NO expiry — an entry lives until the page deletes it or the user clears data — so N origins × 5 MB grows without bound over a browsing lifetime. Real browsers handle this via storage-pressure eviction plus a storage-manager UI; this is the engine half. The per-origin quota in 8.2.1 caps a single origin; this caps the aggregate. Depends on 8.2.1/8.2.2 existing first.)*

- [ ] **[M15 P3] T-STORAGE-ENCRYPTION-1: Encryption At Rest For Profile Data**; Goal: match the shipping-browser bar for cookie/storage confidentiality; Scope: OS-keystore-backed encryption of cookie values and localStorage (DPAPI / libsecret / Keychain), applied over whatever format T-PROFILE-DATA-DIR-1 lands; Acceptance: the on-disk files are not readable without the per-user key; Tests: round-trip through the encryption layer. *(Deliberately LOW priority and honestly scoped: this is shallow protection even in Chrome — the key sits next to the data, tied to the OS login, so it stops another user account but not malware running as you. The profile-dir move above buys more real safety for less work. Do not oversell this as "secure".)*

## Unscheduled - to be filed at a later kickoff

- **CSS Transitions and Animations** (`animation*`, `transition*`, `@keyframes`) are
  unimplemented and not tracked as a story anywhere. They are a subsystem of their
  own, best decided alongside the M14 compositor work. Noted on
  `T-CSS-VISUAL-EFFECTS-1` (M12); file properly at the M14 kickoff.
- **Accessibility tree / screen-reader surface.** Landmark role mapping exists
  (`T-A11Y-ROLE-1`, M5), but no milestone carries an accessibility-tree story. Flag
  at the M11 kickoff if a proof target forces it.
- **On-disk HTTP cache.** M9 ships memory-only by design; file the disk half when
  9.3.1 lands, and pair it with `T-PROFILE-DATA-DIR-1`.
- **`object-position`.** Deferred from M8.5's `object-fit` work because the `Value`
  struct is single-valued and a two-value position needs its own sub-struct and
  parser. Placement is centred until then. Pick up with M10's replaced-element work.
