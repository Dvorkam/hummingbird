# Conformance Registers

One file per spec/module recording **what we actually adhere to**, what we
deliberately deviate from, and what would have to change to close the gap. These
are the punch-lists we would work from if the goal ever shifts from "render our
proof targets" to "render the real web correctly" — see
`doc/dev_guide/spec_conformance_strategy.md` for why we are demo-driven today and
what would trigger that change.

## The one rule that keeps this from becoming clutter

**A thing lives in exactly one place.**

| | Owns | Does not hold |
|---|---|---|
| `doc/TODOs.md` | **Work items.** Things to build, with a `T-*` id. | Spec-adherence narrative. |
| `doc/conformance/*.md` | **The adherence picture.** What conforms, what deviates, why. | Work items. It *links* to `T-*` ids. |

So a gap is described once here and, if it is worth scheduling, linked to one
ticket in `TODOs.md`. Never write the same paragraph in both — that is how two
registers drift and both become untrustworthy. Done right this **shrinks**
`TODOs.md`, because deviation prose moves out of the backlog and the backlog goes
back to being a list of work.

## Anti-rot

A prose-only conformance doc rots into confident lies. Two mitigations:

1. **Pair each register with something executable.** The register explains; a
   test counts. For cookies that is `T-COOKIE-CONFORMANCE-VECTORS-1` — a pinned
   vector table whose pass count is the number. Prose without a number is a
   claim; prose next to `N/M` is a status.
2. **Only create a register when there is real content.** One good file beats
   six stubs. Add the next when a module actually stabilizes, not pre-emptively.

## Review cadence

Registers are reviewed at **milestone kickoff**, alongside the existing scope
validation (see the top of `doc/milestones/milestone8.md` for what that looks
like). Review only the registers a milestone actually touches.

| Register | Review before | Why |
|---|---|---|
| [`rfc6265_cookies.md`](rfc6265_cookies.md) | **M9** (fetch/XHR inherits cookie semantics), **M15** (security model) | M9 adds a second class of request that must obey the same policy; M15 audits the security guarantees. |
| [`html_tag_support.md`](html_tag_support.md) | **M9+** (each real-web target adds tag/property gaps) | The tag/property support surface grows page by page as we render real sites; review when a new proof target lands so its gaps are recorded, not rediscovered. |

Add a row when you add a register. Likely future ones, when their modules
stabilize: URL parsing (RFC 3986 / WHATWG URL — `urltestdata.json` is the
ready-made vector file), the DOM event model (`T-EVENT-SPEC-GAPS-1` is already
the seed), and HTML parsing (html5lib-tests).

## A note on "WPT"

**WPT** = [web-platform-tests](https://github.com/web-platform-tests/wpt), the
shared conformance suite Chrome, Firefox, and Safari all run in CI. It is the
eventual ratchet, but it is **not always the cheapest one** and these registers
are not WPT-specific. WPT is browser-driven: `testharness.js`, a `wptserve`
instance with dedicated host aliases, and many assertions made through `fetch`.
Where a module's core is a pure function, a plain vector table is far cheaper and
tests the same semantics — see the cookie register for that argument worked
through. Use whichever ratchet is cheap for the module; record the number either
way.
