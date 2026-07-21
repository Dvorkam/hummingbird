# Spec Conformance Strategy

This note records *how* Hummingbird relates to web standards (HTML, CSS, DOM,
URL/RFC 3986, etc.) and *when* that relationship should change. It is a strategy
note, not a step-by-step workflow.

## The two modes

There are two ways to build an engine against a standard:

1. **Demo-driven (breadth-first).** Pick high-value proof targets (real pages,
   milestone demos), implement the smallest slice that makes them work, and
   deviate from the spec freely where no target needs the corner. Optimizes for
   *surface area* and *learning*.
2. **Conformance-first (correctness-first).** Treat the standard's test suite as
   the definition of done and drive a pass-rate number upward. Optimizes for
   *interop with the real web* and *trustworthy primitives*.

Hummingbird is deliberately in **mode 1**, and that is the correct choice for an
educational prototype building breadth. This is the same path real engines
(KHTML/WebKit, early Gecko) took — nobody starts by implementing a spec section
to the letter, because most spec corners are dead code no page exercises.

## This is a phase change, not a flip

The move to mode 2 is **not a switch you throw** and **not a rewrite**. It is
gradual and driven by a *cost crossover*: today a new feature buys more than a
correctness fix does. Watch for the day that inverts — when bugs in the
primitives cost more to diagnose than missing features cost to lack, because you
can no longer trust your own foundations. That is the signal, and it arrives
module by module, not all at once.

The external trigger for going conformance-first wholesale is a **change of
goal**: from "render our proof targets + learn" to "render arbitrary real sites
correctly." That is a different project with a different definition of done.

## The mechanism is a test suite, not "reading the spec"

"Follow the standards" in practice does **not** mean read the RFC and code
against the prose. It means **adopt the conformance test suite** as the ratchet:

- For the web platform that is **web-platform-tests (WPT)** — the shared suite
  every production browser runs in CI. Point the engine at a relevant subset,
  get a pass-rate, and features become "move this number up."
- The spec prose is the *reference*; the test suite is the *ratchet*. A number
  that only ever goes up is what stops compliance from rotting.

## What we do now to make the eventual turn cheap

Two cheap disciplines, kept continuously, mean the flip is a non-event when it
comes:

1. **Keep a deviation register.** Every "MVP: we do X, the spec says Y" gets a
   TODO with the deviation stated explicitly (see `doc/TODOs.md`, e.g.
   `T-EVENT-SPEC-GAPS-1`, which enumerates the event-system deviations). When we
   later go conformance-first we then have a punch-list, not a rediscovery
   exercise. **Never skip logging a deliberate deviation.**
2. **Retrofit a conformance slice per module as it stabilizes.** When a module
   settles (e.g. URL parsing, or the event system after its milestone), wire in a
   focused conformance slice for *that module only* and let it be the regression
   floor. Incremental, never big-bang.

## The gap the register does not catch

The deviation register only records deviations we *chose*. It does not catch
**unnoticed correctness bugs** — a primitive that is silently wrong against the
spec nobody flagged as an MVP shortcut. Example: `resolve_url` appended a
fragment-only href to a base that already had a fragment, producing
`…/todo#/active#/completed` (RFC 3986 §5.3 says a `#frag` reference *replaces*
the base fragment). That escaped the register because nobody decided it — it was
just wrong, and surfaced only when a user clicked the todo filters.

This class of bug is the standing argument for the test-suite ratchet: a
conformance suite (WPT) catches spec-violating primitives long before a user
does. Until we adopt one, targeted unit tests on each primitive are the
stopgap — every such bug fix should land with a regression test that pins the
spec-correct behavior.

## Security-relevant deviations are a different class

Most deviations are cosmetic or feature gaps: a missing CSS corner costs
fidelity. A deviation in a **security mechanism** costs a guarantee, and the
difference matters for how it is tracked.

Cookies (M8) are the first module where this bites. `SameSite` is not a rendering
feature — it is the browser's built-in CSRF defense, and `Secure`/`HttpOnly` are
the confidentiality controls around a session. A partial implementation of these
does not degrade gracefully: it looks like protection while providing none, which
is worse than not claiming the feature at all.

Discipline for this class:

- **State the guarantee, not just the gap.** `T-COOKIE-NAV-INITIATOR-1` was first
  filed as "Strict over-sends" (a fidelity framing) when the real defect is "Lax's
  CSRF protection does not apply to cross-site form POSTs" (a guarantee framing).
  The second wording is what makes the priority obvious.
- **Priority follows the guarantee.** A security deviation that silently voids a
  protection is P0 for its milestone, not a nice-to-have.
- **Never describe a partially-enforced control as supported.** Say which half
  works. Cookie SameSite is exact for subresources and inert for navigations, and
  both halves belong in the same sentence.
- **These are the first candidates for a conformance slice.** RFC 6265bis has
  well-defined behavior and WPT has a `cookies/` suite; when the cookie module
  stabilizes it is a better first ratchet than a rendering module, because the
  cost of being quietly wrong is higher.

## Summary

- Stay demo-driven for now; it is the right mode for this phase.
- Log every deliberate deviation — that register is the bridge to compliance.
- Add per-module conformance slices as modules stabilize; do not wait for a
  big-bang flip.
- Adopt WPT (or a curated subset) and make pass-rate a tracked metric once the
  goal shifts to correctly rendering the real web.
