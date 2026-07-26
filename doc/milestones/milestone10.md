> **Status: Planned** — created 2026-07-26 by triaging `doc/TODOs.md` at the M9
> kickoff. Stories were moved here from the backlog; **revalidate every Scope line
> against the codebase at kickoff** (the M8 and M9 kickoffs both found pre-written
> scopes that no longer matched the tree).

## Milestone 10 North Star Deliverable

**Before (after Milestone 9):**

* Pages can fetch and render live data, hold a session, and run scripts — but the
  *visual* model is still M6-era. There are no real scroll containers (only document
  scroll), `position: fixed` does not exist, `z-index` is not honored, and absolute
  positioning ignores static position.
* Layout is recomputed whole on every interaction, and intrinsic sizing is faked
  with a 100000px sentinel probe that downstream code detects by threshold.
* Modern CSS the engine *parses* is silently dropped or, worse, mis-applied: an
  unsupported pseudo-class widens a selector instead of invalidating the rule.

**After (Milestone 10 done):**

* **Positioning:** `position: fixed` (non-negotiable), `sticky`, and correct static
  position for `auto` insets on absolutes.
* **Overflow + Scrolling v2:** real scroll containers with scroll offsets and
  hit-testing through scroll transforms — the other half of M8.5's clip-only MVP.
* **Stacking Context v1:** `z-index` basics, so headers and menus layer sanely.
* **Selector and length correctness:** rules the engine cannot evaluate match
  nothing instead of matching too much; one length resolver instead of five.
* **A real intrinsic-sizing signal** replacing the sentinel-probe idiom.
* **Proof target:** **Wikipedia desktop and old.reddit** render with sticky/fixed
  headers, scrollable panes, and correct layering, and back/forward works through a
  browse session.

*Continuity note:* M9's cross-origin proof already uses Wikipedia's REST API, so
this milestone escalates the same site from "we can read its API" to "we can render
its pages."

---

## Non-Goals

* No compositing, no retained raster, no 60fps guarantee — that is M14. Scrolling
  here must be *correct*, not smooth.
* No CSS transitions/animations (untracked as a subsystem; decide at the M14
  kickoff).
* No SPA navigation beyond M9's History API MVP — the rest stays M12.
* No forms v2, focus, or clipboard work — that is M11.
* No multicol, no `line-clamp` (M11/M12 respectively).

---

## Critical Path (what must land for the proof target)

**Must-have**

* Real scroll containers with offsets and hit-testing (10.2.1) — old.reddit and
  Wikipedia both have scrollable panes, and hit-testing through a scroll transform
  is what makes them clickable rather than merely visible.
* `position: fixed` and `sticky` (10.1.1) — Wikipedia's desktop header and
  old.reddit's chrome.
* `z-index` / stacking v1 (10.3.1) — without it, fixed headers paint under content.
* Static position for `auto` insets (10.1.2) — already diagnosed on the DDG results
  page.
* History baseline: back/forward/reload with a stable document lifecycle
  (no leaks, no dangling `string_view`) — the roadmap calls this out as a
  prerequisite for M12's SPA navigation.

**Foundational (do early — later stories get simpler)**

* `T-LAYOUT-INTRINSIC-SIZE-CONSTRAINT-1` (10.4.1) — the root cause behind three
  separately-patched ballooning bugs. Every percentage/auto sizing story in this
  milestone gets easier once measurement is explicit.
* `T-CSS-SELECTOR-UNSUPPORTED-DROP-1` (10.5.1) — until this lands, every CSS fix is
  measured against a cascade that mis-applies rules, so debugging any layout
  problem on a real page is confounded.

---

## Milestone 10 Done When

* Wikipedia desktop renders with its header behaving correctly on scroll, its
  content pane scrolling independently where the page asks for it, and correct
  layering.
* old.reddit renders a browsable front page and comment page; back/forward through a
  browse session leaves no leaked documents.
* A rule the engine cannot evaluate (`div:first-child`, `a:not(.x)`, `p::before`)
  applies to **no** element.
* No sizing decision keys off a hard-coded ~20000/100000 constant.
* The existing shrink-to-fit / table-intrinsic / replaced-percent regression suite
  stays green against the new intrinsic-sizing signal.

---

## Stories

### 10.1 - Positioning

* **Story 10.1.1: `position: fixed` and `sticky`** *(new story — the roadmap's
  headline item, not previously ticketed)*
* **Goal:** a fixed element positions against the viewport and does not scroll; a
  sticky element sticks within its scroll container.
* **Scope:** positioning + paint + hit-testing. Interacts with 10.2.1 (sticky is
  defined relative to a scroll container) and 10.3.1 (fixed elements almost always
  need a stacking context to paint above content). Sequence: scroll containers →
  fixed → sticky.
* **Acceptance:** Wikipedia's desktop header stays put while the article scrolls,
  and remains clickable.
* **Tests:** positioning + hit-test tests.

* **Story 10.1.2: Static Position For `top:auto`/`left:auto` Absolutes
  (T-CSS-ABS-STATIC-1)** [P2]
* **Goal:** place an absolutely-positioned box with an `auto` inset at its **static
  position** (where it would sit in normal flow) rather than anchoring it to the
  containing block's top-left.
* **Scope:** compute and plumb the static position for absolute boxes and use it
  when the corresponding inset is `auto`.
* **Acceptance:** a `position:absolute; top:auto` box aligns to where it would be in
  flow, not `y=0` of its containing block.
* **Tests:** positioning tests.
* *Filed 2026-07-16 via F1 inspect on the DDG results page: the header logo
  (`.header__logo-wrap{position:absolute; margin-top:-8px}`, `top:auto`) renders at
  `rect.y=56` instead of the flow-aligned ~65, ~9px too high. Also named as a
  contributing cause of the seznam.cz overlap class (B) — M8.5's clip work reduced
  that bleed but explicitly did not finish it.*

* **Story 10.1.3: `inset` Shorthand (T-CSS-INSET-SHORTHAND-1)** [P3]
* **Goal:** expand `inset`/`inset-block`/`inset-inline` to the four physical
  longhands instead of dropping them.
* **Scope:** parse-time shorthand expansion; the longhands already exist. `inset`
  currently warns "unsupported" and is ignored, so an overlay using
  `position:absolute; inset:0` gets no offsets and anchors to the containing block's
  top-left at intrinsic size.
* **Acceptance:** `inset:0` on an absolute box stretches it to its containing block.
* **Tests:** shorthand-expansion + positioning tests.
* *Filed 2026-07-23. Small; rides with this milestone's positioning work.*

### 10.2 - Overflow + Scrolling v2

* **Story 10.2.1: Real Scroll Containers** *(new story — the roadmap's headline
  item; M8.5 shipped the clip-only half)*
* **Goal:** an `overflow: auto|scroll` box scrolls its own content, with correct
  hit-testing through the scroll offset.
* **Scope:** the deliberate boundary M8.5 drew: `RenderObject::paint` already clips
  when **both** overflow axes are `Hidden`, and single-axis hidden plus
  `scroll`/`auto` were left unclipped precisely because a rectangular clip cannot
  express an unbounded axis and there are no scroll offsets yet. This story adds the
  offsets, which unlocks: single-axis clipping, `overflow-x:hidden` done properly,
  scrollbar affordances, and scroll-aware hit-testing. Sticky positioning (10.1.1)
  is defined against these containers.
* **Acceptance:** an `overflow:auto` box with oversized content scrolls
  independently of the document, and clicks inside it land on the element under the
  cursor at the current scroll offset.
* **Tests:** scroll-container layout, paint, and hit-test tests.

### 10.3 - Stacking

* **Story 10.3.1: Stacking Context v1 (`z-index`)** *(new story — roadmap headline)*
* **Goal:** honor `z-index` for positioned elements so headers and menus layer
  predictably.
* **Scope:** paint-order model over the display list; positioned + `z-index`
  establishes a stacking context. Sequenced with 10.1.1 because a fixed header that
  paints beneath content is indistinguishable from a fixed header that does not work.
* **Acceptance:** a `position:fixed; z-index:10` header paints above in-flow content.
* **Tests:** display-list ordering tests.

### 10.4 - Layout Model Correctness

* **Story 10.4.1: A Real Intrinsic-Sizing Signal (T-LAYOUT-INTRINSIC-SIZE-CONSTRAINT-1)**
  [P2] — **foundational**
* **Goal:** replace the "lay children out at a huge sentinel width and read the
  result back" idiom with an explicit max-content/min-content measurement mode, so
  measurement code never has to *infer* that it is measuring from a suspiciously
  large width.
* **Scope:** the engine simulates unbounded space with
  `kInlineAtomicLayoutWidth`/`kIntrinsicMeasureWidth` = **100000px** (`BlockBox.cpp`,
  `FlexBox.cpp`, `RenderTable.cpp`), then detects the probe downstream by threshold:
  `resolve_table_target_width` treats `available_width >= 20000` as auto, and 8.5.1's
  `resolve_length` treats `percent_basis >= kIntrinsicMeasureThreshold (20000)` as
  indefinite. That threshold is a proxy for a signal the layout API does not carry.
  Thread a `SizeConstraint { Definite, MinContent, MaxContent }` (or a `measuring`
  flag) through `RenderObject::layout` and the flow helpers so percentage and `auto`
  resolution branch on the real mode, and retire the magic thresholds.
* **Acceptance:** intrinsic measurement is requested explicitly, no sizing decision
  keys off a hard-coded ~20000/100000 constant, and the existing balloon-prevention
  tests still pass.
* **Tests:** the current shrink-to-fit / table-intrinsic / replaced-percent
  regression suite stays green against the new signal.
* *Filed 2026-07-23 — the user flagged the `>= 20000` guard as "feels like a gap
  even though it's needed." It is: a shared architectural shortcut, not a new bug.
  The symptom fixes `T-LAYOUT-SHRINK-TO-FIT-1` and
  `T-LAYOUT-TABLE-INTRINSIC-BLOCK-1` patched specific ballooning; this is the root.
  Do it early in the milestone: 10.4.2 and 10.4.3 both get simpler, and it also
  subsumes `T-CSS-REPLACED-PERCENT-INLINE-1`'s missing-context problem.*

* **Story 10.4.2: Propagate Definite Containing-Block Heights
  (T-CSS-PERCENT-HEIGHT-PROPAGATE-1)** [P2]
* **Goal:** make `height: N%` resolve against a block ancestor that has a definite
  height.
* **Scope:** `FlowLayout::layout_block_child` (`src/layout/flow/FlowLayoutUtils.cpp`)
  hard-codes the child's `bounds.height = 0.0f`, so a block child never receives a
  containing-block height; `resolve_height_constraint` then ignores the percentage
  and the replaced-element path correctly treats it as `auto`, even when the parent
  *does* have a definite height that was lost in transit. Thread the parent's
  definite content height down as `bounds.height`, leaving it 0 when the parent
  height is auto (percentages stay indefinite, per CSS). This is the missing half of
  8.5.1: percentage *width* resolves because the containing block's width is always
  known at layout time; percentage *height* does not.
* **Acceptance:** `<div style="height:200px"><div style="height:50%">` is 100px, and
  an `<img style="width:100%;height:100%">` in a fixed-size cell fills it rather
  than merely degrading via its intrinsic ratio.
* **Tests:** block + replaced percent-height layout tests.
* *Filed 2026-07-23 from M8.5 in-app testing (a `height:100%` icon rendered 44×100
  instead of 44×44). Related: 10.4.1, 10.4.4.*

* **Story 10.4.3: Percentage Sizing For Inline Replaced Elements
  (T-CSS-REPLACED-PERCENT-INLINE-1)** [P2]
* **Goal:** make `width:100%` / `max-width:100%` resolve on a default
  (inline/inline-block) `<img>`, not only a block-level one.
* **Scope:** `RenderImage`/`RenderSvg::measure_inline` call `compute_layout_size`
  with no containing-block basis, so every percentage on an inline replaced element —
  including the ubiquitous responsive `img { max-width:100% }` idiom — collapses to
  its bare magnitude (`max-width:100%` becomes `max-width:100px`). The caller (the
  inline line builder, driven from `BlockBox::layout`) does know the containing
  block's content width; thread it through `IInlineParticipant::measure_inline`
  (touches InlineBox/InlineBlockBox/TextBox/RenderImage/RenderSvg) — **or fold it
  into 10.4.1**, which is the same missing-context problem and would carry the
  definite-vs-probe distinction for free.
* **Acceptance:** an inline `<img style="width:100%">` in a 40px block is 40px wide,
  and `max-width:100%` clamps to the container instead of 100px.
* **Tests:** inline replaced sizing tests.
* *Filed 2026-07-23 from the M8.5 external review. NOT a regression — the inline
  path used the same fallback before 8.5.1; the block/flex paths that the seznam
  icons needed do resolve.*

* **Story 10.4.4: `aspect-ratio` Box Sizing (T-CSS-ASPECT-RATIO-1)** [P3]
* **Goal:** derive one box dimension from the other plus a ratio.
* **Scope:** `aspect-ratio` warns "unsupported". When one axis is definite and the
  other is `auto`, compute the auto axis from `width/height = ratio`. Parse the
  property and apply it in box + replaced sizing after the definite axis is known.
  Note that 8.5.1 already implemented *intrinsic* aspect-ratio preservation for
  replaced elements (CSS 2.1 §10.3.2) including re-derivation after box-sizing and
  min/max clamps — this story is the *author-specified* ratio, and should reuse that
  machinery rather than duplicate it.
* **Acceptance:** a box with `width:200px; aspect-ratio:2/1` resolves to 100px tall;
  a replaced element with a ratio and one definite axis sizes the other.
* **Tests:** box + replaced sizing tests.

### 10.5 - CSS Correctness

* **Story 10.5.1: Unsupported Selectors Must Drop The Rule
  (T-CSS-SELECTOR-UNSUPPORTED-DROP-1)** [P1] — **foundational**
* **Goal:** make a selector the engine cannot faithfully evaluate match *nothing*,
  as CSS requires, instead of matching *more* than the author wrote.
* **Scope:** `Parser::parse_simple_selector` recognizes exactly four pseudo-classes
  (`:hover`, `:active`, `:focus`, `:visited`) and **silently discards every other
  one**, keeping the rest of the compound. Per CSS Syntax, an invalid or unsupported
  selector invalidates the whole rule. Three consequences, all verified against the
  build on 2026-07-24:
  - **over-match** — `div:first-child { … }` becomes plain `div` and styles *every*
    div (confirmed: the second div received the rule);
  - **inverted match** — because the tokenizer also drops parentheses
    (`T-CSS-PAREN-TOKENS-1`), `a:not(.hidden) { … }` parses as `a.hidden` and styles
    exactly the elements the author excluded (confirmed);
  - **pseudo-element leakage** — `p::before { … }` degrades to `p`, so declarations
    meant for a generated box land on the real element.
  Add a validity flag to the selector parser (unknown pseudo-class, unknown
  pseudo-element, or a functional pseudo whose arguments cannot be parsed ⇒ invalid)
  and skip such rules at `parse_one_rule`, ideally recording them through
  `CompatibilityWarnings` so the gap is visible rather than silent.
* **Acceptance:** `div:first-child`, `a:not(.hidden)`, and `p::before` rules apply to
  no element until each is genuinely implemented; the four supported pseudo-classes
  are unaffected.
* **Tests:** selector-parser validity tests per shape + a style test that the rules
  match nothing.
* **Related oddity to settle in the same pass:** a selector that *starts* with `:`
  is not a selector-start token, so `:root { … }` falls through the recovery path,
  the bare `:` is consumed, and the remainder parses as a **tag selector `root`** —
  which is the exact name of the HTML parser's synthetic wrapper element. So `:root`
  works today entirely by coincidence (verified: `:root{font-size:20px; --pad:7px}`
  correctly reaches every element by inheritance). Whatever this story does about
  validity **must keep `:root` working on purpose**, as a real pseudo-class matching
  the document root element, rather than by that accident.
* *Filed 2026-07-24 from the M8 pre-merge review. Pre-existing, not introduced by
  M8, but it belongs early here: contemporary CSS leans on `:not()`, `:nth-child()`,
  `:first-child`, and `::before`/`::after` constantly, so the engine currently does
  not merely miss those rules — it applies them to the wrong elements. Until it is
  fixed, every other layout fix in this milestone is measured against a cascade that
  lies.*

* **Story 10.5.2: Tokenize Parentheses (T-CSS-PAREN-TOKENS-1)** [P3]
* **Goal:** parse CSS functional notation with correct boundaries, so
  `grid-template-columns: repeat(2, 1fr) 100px` (repeat not last) and `minmax()`
  work.
* **Scope:** the CSS tokenizer *drops* `(`/`)` (only `url(...)` is special-cased), so
  functions cannot be delimited — the grid MVP works around this by having
  `repeat()` greedily consume the rest of the track list. Emit `LParen`/`RParen`
  tokens (or capture the raw value span) and update the paren-agnostic consumers —
  `parse_calc`, colour functions, `read_one_grid_track`,
  `parse_grid_track_list_text` — to honor them; then support `repeat()` anywhere and
  add `minmax(min, max)`.
* **Acceptance:** `repeat(2, 1fr) 100px` yields 3 tracks and `repeat()` may appear
  before other tracks; `minmax(100px, 1fr)` clamps correctly.
* **Tests:** parser + grid layout tests.
* *Filed 2026-07-16 as a grid follow-up from `T-LAYOUT-GRID-1` (M6): the
  `// repeat() must be the last track-list component` limitation in CssParser is a
  symptom of the dropped-parens tokenizer, not a grid-specific one. Fixing it
  centrally also cleans up `calc()` and colour-function parsing.*
* **Blocks:** 10.6.2 (`grid-template-areas`) needs it, and 10.5.1 needs it to
  validate functional pseudo-classes properly. **Also blocks var() fidelity:** with
  parens dropped, `font-family: var(--x), georgia` (a family list after the var) is
  indistinguishable from `var(--x, georgia)` (a var fallback); the font-family var
  path shipped in M8.5 necessarily reads it as the fallback form, so trailing
  families are dropped when the var resolves. Only real paren tokens can
  disambiguate. **Consider promoting to P2 and doing it before 10.5.1.**

* **Story 10.5.3: Centralize Computed Length Resolution (T-CSS-LENGTH-RESOLVER-1)**
  [P3] *(refactor)*
* **Goal:** make every CSS length consumer use one typed resolver instead of
  repeating unit switches.
* **Scope:** grid tracks, shadows, legacy `clip`, transform token parsing, and
  background identifier shorthands still decode px/em/rem independently even though
  ordinary properties now share `LengthResolutionContext`. Move those value shapes
  onto the same typed conversion API without changing behavior.
* **Acceptance:** adding a future absolute or font-relative unit changes one
  resolver rather than ApplyLayout + ApplyBackground + ApplyText.
* **Tests:** the existing parser/style length matrix stays green.
* *Filed 2026-07-24 while completing `rem`: threading the root-font context exposed
  shotgun unit handling across string-backed shorthands.*

* **Story 10.5.4: Route `consume_declaration` Value Parsing Through The Registry
  (T-CSS-DECL-DISPATCH-1)** [P2] *(refactor)*
* **Goal:** cut the single highest-complexity function in the tree down to a thin
  dispatcher.
* **Scope:** `Css::Parser::consume_declaration` (`src/style/parser/CssParser.cpp`)
  is **CCN 206 / 486 NLOC** — by far the worst hotspot (next is 117). Despite the
  Phase-4 property registry, it still parses most shorthands
  (margin/padding/outline/background/font/grid/transform/gap) and many longhands
  inline via if/else chains, two switches, and ~20 `parse_*` helpers, so every new
  property's value grammar means another branch here. Give each shorthand/longhand
  value parser a named function and dispatch through the registry `ParserHook` (or a
  shorthand-expander table).
* **Acceptance:** `consume_declaration` drops below ~CCN 40; adding a property's
  value parsing is a registry/table edit.
* **Tests:** existing `CssParser`/registry tests stay green (behavior-preserving).
* ***Riskiest refactor in the tree; do NOT attempt near a release.*** *Sequence it
  early in the milestone, not late, and not alongside 10.5.2 (which touches the same
  file for a different reason).*

### 10.6 - Flex and Grid Completion

*10.6.1 and 10.6.2 close the seznam.cz modern-portal gaps that M8.5 explicitly left
open; 10.6.3 and 10.6.4 were filed while completing `T-DDG-LAYOUT-1` (M6), which
shipped row wrap and a font-metric baseline approximation and deferred the rest.*

* **Story 10.6.1: Flex Alignment Surface (T-CSS-FLEX-ALIGNMENT-2)** [P2]
* **Goal:** complete the flex alignment surface M6's v1 left out.
* **Scope:** M6 shipped direction, basic `align-items`/`justify-content`, and row
  wrap, but `align-self` (per-item cross-axis override), `align-content`
  (multi-line packing), `justify-self`, and the `flex-flow` shorthand are all
  dropped (each warns "unsupported"), and FlexBox ignores `row_gap`/`column_gap`
  even though the fields exist on `ComputedStyle` (added for grid). On seznam these
  no-ops collapse flex rows so items pile at main-start and lose their spacing. Add
  the missing fields to `ComputedStyle`, parse them (expand `flex-flow` to
  direction+wrap), and apply in `FlexBox`.
* **Acceptance:** an item with `align-self:flex-end` moves independently of its
  siblings, a wrapped container honors `align-content`, and `gap` spaces items.
* **Tests:** flex layout tests.

* **Story 10.6.2: Full Grid — Template Areas, Named Areas, Auto-Placement
  (T-CSS-GRID-TEMPLATE-AREAS-1)** [P2]
* **Goal:** lay out grids defined by named areas and auto-flow, not just explicit
  numeric tracks.
* **Scope:** the grid MVP shipped explicit `grid-template-columns/rows` placement,
  but `grid-template`/`grid-template-areas`, `grid-area: <name>`, line-name
  resolution, and auto-placement of un-positioned items are all missing (each warns
  "unsupported"); seznam builds whole page sections with `grid-template-areas`, so
  they collapse to a single stack. Parse the area/template grammar, resolve named
  areas to line ranges, and implement auto-flow placement.
* **Acceptance:** a container with `grid-template-areas` + children using
  `grid-area:<name>` places each child in its named region, and un-placed children
  auto-flow.
* **Tests:** grid layout tests over named areas + auto-placement.
* **Depends on 10.5.2** (real `repeat()`/`minmax()` tokenizing) — real templates use
  them. *The single biggest layout item in the seznam cluster.*

* **Story 10.6.3: Column-Direction Flex Wrap (T-LAYOUT-FLEX-WRAP-COL-1)** [P2]
* **Goal:** support `flex-wrap` for `flex-direction: column` containers, which M6
  only models for rows.
* **Scope:** FlexBox line-breaking against a definite column main size (height) and
  placing wrapped lines as side-by-side columns, with the container cross size
  (width) growing to fit.
* **Acceptance:** a column flex container with a fixed height and enough items wraps
  into multiple columns instead of one overflowing column.
* **Tests:** flex layout tests.

* **Story 10.6.4: Accurate Flex Baseline From Nested Content
  (T-LAYOUT-FLEX-BASELINE-2)** [P3]
* **Goal:** derive a flex item's baseline from its first in-flow line box rather
  than approximating from the item's own font metrics.
* **Scope:** expose a first-baseline query on RenderObject/line boxes and have
  FlexBox `align-items: baseline` use it (also covers column cross-baseline).
* **Acceptance:** an item whose text lives in a nested child aligns on that child's
  baseline.
* **Tests:** flex baseline tests.

### 10.7 - Inline and Table Gaps

* **Story 10.7.1: Inline Horizontal Margins + Inter-Element Whitespace
  (T-CSS-INLINE-SPACING-1)** [P2]
* **Goal:** put visible space between adjacent inline elements, from both
  `margin-left`/`margin-right` on inline boxes and collapsed source whitespace.
* **Scope:** inline links laid out side by side currently abut with no gap — the M7
  `example.dev/todo` filter bar rendered as "AllActiveCompleted" (worked around
  there with a flex row, whose *item* margins are honored). Two sub-gaps: horizontal
  margins on inline boxes are ignored by the inline line builder, and whitespace-only
  text between inline elements is dropped instead of collapsing to a single space.
* **Acceptance:** `<a>` links separated by whitespace and/or `margin-right` show a
  gap.
* **Tests:** inline layout tests.

* **Story 10.7.2: `border-collapse` / `border-spacing`
  (T-CSS-TABLE-BORDER-MODEL-1)** [P3]
* **Goal:** honor the two table border-model properties instead of always rendering
  the separated model at a fixed gap.
* **Scope:** both warn "unsupported". Implement `border-collapse:collapse` (shared
  single border between cells — the more common request) and `border-spacing`
  (explicit gap in the separated model) in the table layout/paint path.
* **Acceptance:** a `border-collapse:collapse` table draws one border between
  adjacent cells; `border-spacing:Npx` sets the separated-model gap.
* **Tests:** table layout + paint tests.
* *Appears widely — HN and seznam both hit it — but low visual impact next to the
  sizing/positioning items.*

### 10.8 - Tech Debt Riding Along

*Each of these is a small, low-risk cleanup best paid at its next natural touch
point. They are listed here because this milestone touches the files in question;
none is a milestone gate.*

* **10.8.1: DOM Node Should Not Point At Computed Style
  (T-DOM-STYLE-COUPLING-1)** [P3] — remove the inward-depends-outward edge
  `core/dom -> style/types`. `DOM::Node` caches a `shared_ptr<ComputedStyle>`, so the
  innermost layer depends on the style layer. Consider a side table
  (node → computed style) owned by the style/engine layer. *Filed 2026-07-17 from the
  `T-ARCH-GUARD-2` package diagram (see `doc/dev_guide/architecture_diagrams.md`).
  Touches every computed-style read site — do it before M12 multiplies those sites,
  and not in the same pass as 10.4.1.*
* **10.8.2: JS `URL` Polyfill Has The Bug `Core::resolve_url` Already Fixed
  (T-URL-POLYFILL-DIVERGENCE-1)** [P3] — `QuickJSScriptEngine.cpp`'s `URLImpl`
  reimplements URL resolution in JS/regex instead of calling the native
  `Core::resolve_url`, so `new URL('#frag', base)` still drops the base query — the
  exact bug the native path fixed in `05efc9e`. Two resolvers with diverging
  behavior is itself the risk. *Note: M9's fetch work makes URL resolution
  load-bearing in JS, so re-check the priority at M10 kickoff — this may want
  promoting.*
* **10.8.3: Three Copies Of HTML-Escaping Logic (T-HTML-ESCAPE-DEDUP-1)** [P3] —
  `DocumentScriptHost.cpp` (innerHTML), `DocumentResources.cpp` (SVG), and
  `BookmarkStore.cpp` each hand-roll the same escaping. **Now four:**
  `NetworkErrorPage.cpp` added a fourth copy in M8. One helper in `core/utils`,
  four call sites.
* **10.8.4: Checkbox-Type Detection Re-Derived (T-CHECKBOX-DETECT-DEDUP-1)** [P3] —
  `BlockBox.cpp`'s `is_checkbox_input` and `StyleDefaults.cpp`'s
  `input_type_is_toggle` re-derive what `DocumentInputUtils::is_checkbox_input_element`
  already exports. *M8 set the precedent for the fix by consolidating
  `is_text_control_tag` into one list during the textarea rework.*
* **10.8.5: Selector Strings Re-Parsed On Every `querySelector`
  (T-QUERYSELECTOR-CACHE-1)** [P3] — `DocumentScriptHost::parse_selector_list`
  re-runs the CSS tokenizer/parser on every call with no cache; a render-on-mutation
  page reparses the same CSS on every keystroke. *Related to
  `T-PERF-LAYOUT-INCREMENTAL-1` (M13).*
* **10.8.6: Bookmark Store I/O On The Navigation Path
  (T-BOOKMARK-STORE-IO-1)** [P3] — `BookmarkStore::add`/`save` truncate-and-rewrite
  the whole TSV per add, and `ResourceLoader` constructs a fresh `BookmarkStore`
  (re-reading the file) on every `about:bookmarks` navigation instead of reusing
  `BrowserApp`'s loaded instance.
* **10.8.7: Repeated Binding Boilerplate (T-QUICKJS-BINDING-DEDUP-1)** [P3] — ~25
  binding functions repeat an identical "context lookup → null-check → node resolve →
  delegate" skeleton; `js_token_list_add`/`remove` are identical but for the method;
  the query-selector family shares an arg-validation preamble; `Tab.cpp` copy-pastes
  its history-record guard at 4 sites. *M9 adds fetch/XHR bindings on top of this
  boilerplate — if M9 makes it worse, promote this.*
* **10.8.8: Collapse Repeated Field-Parse Block In ExtensionManifest
  (T-EXT-MANIFEST-FIELD-HELPER-1)** [P3] — the same skip-whitespace / match-quote /
  set-error pattern repeats ~7× in `ExtensionManifest.cpp`. **Bundle with M9's 9.4.1**,
  which adds manifest fields for declarative rules and permission enforcement —
  that is the natural touch point.
* **10.8.9: Click Handling Can Trigger Two Style/Layout Passes
  (T-INVALIDATION-BUDGET-CLICK-DOUBLE-PASS-1)** [P3] — `DocumentEventRouter`'s
  mouse-down handler calls `Tab::dispatch_click` (which rebuilds if the handler
  mutated the DOM) and then `Tab::refresh_styles_for_interaction` (which rebuilds
  again), so a click that both mutates and changes focus spends two passes on one
  input task. Undercuts 7.4.1's one-pass-per-task guarantee in the general case.

---

## Execution Order Checklist

Foundational (do first — everything downstream gets simpler or more trustworthy)
- [ ] 10.5.2: Tokenize Parentheses *(consider promoting to P2; blocks 10.5.1 and 10.6.2)*
- [ ] 10.5.1: Unsupported Selectors Must Drop The Rule
- [ ] 10.4.1: A Real Intrinsic-Sizing Signal

P0: Proof target (Wikipedia desktop + old.reddit)
- [ ] 10.2.1: Real Scroll Containers
- [ ] 10.3.1: Stacking Context v1 (`z-index`)
- [ ] 10.1.1: `position: fixed` and `sticky`
- [ ] 10.1.2: Static Position For `auto` Insets
- [ ] History baseline: back/forward/reload with a stable document lifecycle
      *(roadmap item; needed before M12's SPA navigation — write it up as a story at kickoff)*

P1: Layout model + CSS correctness
- [ ] 10.4.2: Propagate Definite Containing-Block Heights
- [ ] 10.4.3: Percentage Sizing For Inline Replaced Elements *(may fold into 10.4.1)*
- [ ] 10.6.1: Flex Alignment Surface
- [ ] 10.6.2: Full Grid — Template Areas + Auto-Placement
- [ ] 10.7.1: Inline Horizontal Margins + Whitespace
- [ ] 10.5.4: Route `consume_declaration` Through The Registry *(risky; early, not late)*

P2/P3: If schedule allows
- [ ] 10.1.3: `inset` Shorthand
- [ ] 10.4.4: `aspect-ratio`
- [ ] 10.6.3: Column-Direction Flex Wrap
- [ ] 10.6.4: Flex Baseline From Nested Content
- [ ] 10.7.2: `border-collapse` / `border-spacing`
- [ ] 10.5.3: Centralize Computed Length Resolution
- [ ] 10.8.x: Tech debt riding along (nine items; each at its natural touch point)
