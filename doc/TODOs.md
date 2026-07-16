# TODOs

## Milestone 3 (Navigator)

Milestone complete. See `doc/todo_archive/milestone3_done.md`.

## Milestone 4 (Scripting)

Milestone defined in `doc/milestones/milestone4.md` (stories moved there).

### Additional M4 blockers (DDG HTML)

- [x] **[M5 P1] T-CSS-PERCENT-1: Percentage Unit Parsing/Storage**; Goal: stop mis-parsing `%` values as plain numbers; Scope: tokenizer/parser + style value representation for `%` (e.g. `70%`, `100%`, `24%`) without silently degrading to px-like values; Acceptance: `%` values are preserved through parse/apply pipeline and unit tests cover parse of `%` in width/offset properties; Tests: CSS parser/style tests.
- [x] **[M5 P1] T-LAYOUT-PERCENT-1: Percentage Width/Height Resolution**; Goal: resolve `%` against containing block dimensions for core box sizing; Scope: layout metrics + block/replaced sizing for width/height/min/max where applicable; Acceptance: common patterns (`width:70%`, `width:100%`) render proportionally instead of fixed small widths; Tests: layout tests + DDG smoke fixture assertion.
- [x] **[M5 P1] T-LAYOUT-PERCENT-POS-1: Percentage Offsets For Positioned Elements**; Goal: make `top/right/bottom/left` `%` offsets behave consistently for positioned elements; Scope: positioning resolution path for absolute/relative offsets; Acceptance: `top:24%`-style layouts position as expected relative to containing block; Tests: positioning tests.
- [x] **[M5 P1] T-FORM-HIT-1: Input Click Must Not Trigger Submit**; Goal: prevent accidental form submit when user clicks editable text fields; Scope: tighten form-submit hit semantics and add regression around DDG home form; Acceptance: clicking text input focuses caret and does not navigate; submit occurs only on submit control click or Enter; Tests: tab/document form interaction tests.

## Milestone 5 (The Architect: Tabs + Extensions)

Milestone complete for `0.5.0` release scope. Archived in `doc/todo_archive/milestone5_done.md`.
Deferred DDG parity follow-ups were moved to Milestone 6.

## Milestone 6 (The Layouter)

Milestone defined in `doc/milestones/milestone6.md` (stories moved there).

## Milestone 7 (The Programmable Document)

Milestone defined in `doc/milestones/milestone7.md`.

## Milestone 8 (The Session Keeper)

Milestone defined in `doc/milestones/milestone8.md`.

## Milestone 9 (The Fetcher)

Draft defined in `doc/milestones/milestone9.md` — revalidate scope at kickoff.

## Milestone 10+ (Later)

- [ ] **[M12 P1] T-DOM-CUSTOM-1: Custom Elements Upgrade** (M12 Framework Gauntlet; required for YouTube/Polymer); Goal: allow JS `customElements.define()` to upgrade dash-named tags and run lifecycle hooks; Scope: DOM + JS bindings; Acceptance: defined custom element runs constructor/connectedCallback and can attach shadow/DOM; Tests: DOM/JS integration tests.

### DDG results-page gaps (found evaluating a live search 2026-07-16)

- [ ] **[M11 P1] T-FORM-SELECT-1: Native `<select>` Dropdown Control**; Goal: render and operate `<select>`/`<option>` controls (DDG results page: the "All Regions" / "Any Time" filters currently fall back to a generic element, leaking their option text as a smushed vertical column against the right edge); Scope: recognize `<select>`/`<option>` as controls, render a closed dropdown box showing the selected option, and (with forms v2 interaction) open/pick options; Scope note: `<option>` text must not render as flow content when closed; Acceptance: a `<select>` renders as a single control box with the selected label, not a stack of option texts; Tests: control layout + form value tests. *(Filed while evaluating the DDG results page; out of M6 scope — M6 form controls were bounded to the homepage's input+button. Best fit: M11 "Inputter" / forms v2.)*
- [ ] **[M10 P2] T-CSS-OBJECT-FIT-1: `object-fit` / `object-position` For Replaced Elements**; Goal: honor `object-fit` (`contain`/`cover`/`fill`/`none`) and `object-position` when sizing replaced elements like `<img>` (DDG results page: `.result__image__img { object-fit: cover }` on result thumbnails; currently unsupported so such images render at intrinsic size within their box); Scope: parse the properties + apply them in replaced-element sizing/paint; Acceptance: an `<img>` with a fixed box and `object-fit: contain`/`cover` scales within the box instead of overflowing; Tests: replaced-element layout tests. *(Filed 2026-07-16; standard properties, no prior story. The DDG header logo overflow initially looked like this but was actually a `background-size: auto <length>` bug, fixed in 98c15ec — object-fit remains a genuine gap for result thumbnails.)*

### Layouter v2 refinements (deferred from M6, flexbox non-goals)

- [ ] **[Later P2] T-LAYOUT-FLEX-WRAP-COL-1: Column-Direction Flex Wrap**; Goal: support `flex-wrap` for `flex-direction: column` containers (multi-column), which M6 only models for rows; Scope: FlexBox line-breaking against a definite column main size (height) and placing wrapped lines as side-by-side columns, with the container cross size (width) growing to fit the columns; Acceptance: a column flex container with a fixed height and enough items wraps into multiple columns instead of one overflowing column; Tests: flex layout tests. *(Filed while completing T-DDG-LAYOUT-1; row wrap shipped, column wrap collapses to a single line.)*
- [ ] **[Later P3] T-LAYOUT-FLEX-BASELINE-2: Accurate Flex Baseline From Nested Content**; Goal: derive a flex item's baseline from its first in-flow line box rather than approximating from the item's own font metrics; Scope: expose a first-baseline query on RenderObject/line boxes and have FlexBox `align-items: baseline` use it (also covers column cross-baseline); Acceptance: an item whose text lives in a nested child aligns on that child's baseline, not the item's synthesized font baseline; Tests: flex baseline tests. *(Filed while completing T-DDG-LAYOUT-1; current baseline uses the item's own font metrics, which is off for items with differently-styled nested content.)*
