# TODOs

## Milestone 3 (Navigator)

Milestone complete. See `doc/todo_archive/milestone3_done.md`.

## Milestone 4 (Scripting)

Milestone defined in `doc/milestones/milestone4.md` (stories moved there). The
"Additional M4 blockers (DDG HTML)" were completed during M5 and archived in
`doc/todo_archive/milestone5_done.md` (2026-07-17).

## Milestone 5 (The Architect: Tabs + Extensions)

Milestone complete for `0.5.0` release scope. Archived in `doc/todo_archive/milestone5_done.md`.
Deferred DDG parity follow-ups were moved to Milestone 6.

## Milestone 6 (The Layouter)

Milestone defined in `doc/milestones/milestone6.md` (stories moved there).

## Milestone 7 (The Programmable Document)

Milestone defined in `doc/milestones/milestone7.md`. Scope revalidated at kickoff
(2026-07-17); T-UI-NAV-BACK-1 and a bookmarks MVP were folded in as chrome stories.

## Milestone 8 (The Session Keeper)

Milestone defined in `doc/milestones/milestone8.md`.

## Milestone 9 (The Fetcher)

Draft defined in `doc/milestones/milestone9.md` — revalidate scope at kickoff.

## Milestone 10+ (Later)

- [ ] **[M12 P1] T-DOM-CUSTOM-1: Custom Elements Upgrade** (M12 Framework Gauntlet; required for YouTube/Polymer); Goal: allow JS `customElements.define()` to upgrade dash-named tags and run lifecycle hooks; Scope: DOM + JS bindings; Acceptance: defined custom element runs constructor/connectedCallback and can attach shadow/DOM; Tests: DOM/JS integration tests.

### Perf/memory architecture (moved out of M6 2026-07-16 — "pull in only if real pages force it"; nothing forces them yet, and each has a natural home with the workload that validates it)

- [ ] **[M14 P1] T-PERF-4: Offscreen Raster Cache + Layer Invalidation**; Goal: repaint only dirty regions instead of the whole frame; Scope: renderer + engine invalidation (cache rasterized layers, invalidate on change); Acceptance: cached layers reused across frames; Tests: renderer perf tests. *(Moved from M6 to M14 The Compositor: this is compositor/retained-rendering work, which M6's own non-goals explicitly exclude; the roadmap puts raster strategy + perf-budget CI assertions at M14.)*
- [ ] **[M14 P2] T-CACHE-1: Tab Resource Eviction + Rehydrate**; Goal: evict resources/render tree for background tabs and restore on focus; Scope: Tab/TabManager + ResourceStore; Acceptance: inactive tabs drop memory and reload on activation; Tests: engine tests. *(Moved from M6: tab-memory architecture, not forced by any current page. Grouped with the M14 perf/retained-rendering track; pull forward to a dedicated perf pass if multi-tab memory becomes a real problem sooner.)*
- [ ] **[M12 P1] T-DOM-1: Infinite Scroll DOM Virtualization**; Goal: cap live DOM/resources for unbounded feeds; Scope: DOM/layout + resource eviction; Acceptance: long feeds do not grow memory unbounded and rehydrate when revisiting content; Tests: engine perf tests. *(Moved from M6 to M12 Framework Gauntlet: needs an SPA/infinite-feed to force and validate it — m.youtube's app shell is exactly M12's proof target.)*

### Grid follow-ups (from T-LAYOUT-GRID-1, 2026-07-16)

- [ ] **[M10 P3] T-CSS-PAREN-TOKENS-1: Tokenize Parentheses (finish grid repeat(), enable minmax())**; Goal: parse CSS functional notation with correct boundaries so `grid-template-columns: repeat(2, 1fr) 100px` (repeat not last) and `minmax()` work; Scope: the CSS tokenizer currently *drops* `(`/`)` (only `url(...)` is special-cased), so functions can't be delimited — the grid MVP works around this by having `repeat()` greedily consume the rest of the track list (must be the last component). Emit `LParen`/`RParen` tokens (or capture the raw value span) and update the paren-agnostic consumers — `parse_calc`, color functions, `read_one_grid_track`/`parse_grid_track_list_text` — to honor them; then support `repeat()` anywhere in the list and add `minmax(min, max)` track sizing; Acceptance: `repeat(2, 1fr) 100px` yields 3 tracks and `repeat()` may appear before other tracks; a `minmax(100px, 1fr)` column clamps correctly; Tests: parser + grid layout tests. *(Filed 2026-07-16: the `// repeat() must be the last track-list component` limitation in CssParser is a symptom of the dropped-parens tokenizer, not grid-specific. Fixing it centrally also cleans up calc()/color function parsing. Candidate M10 assigned 2026-07-17: Layouter II's desktop targets are where grid/calc fidelity starts to matter; reevaluate at M10 kickoff.)*

### Web font follow-ups (from T-FONT-FACE-1, 2026-07-16)

- [ ] **[M9 P2] T-FONT-WOFF2-1: WOFF/WOFF2 Web Font Decoding**; Goal: decode WOFF and WOFF2 web fonts so real icon/text web fonts render (DDG's `ddg-serp-icons` magnifier glyph is WOFF2); Scope: add a WOFF2 decoder (Google `woff2` + `brotli` — note `brotlidec`/`brotlicommon` are already pulled in by libcurl) and a WOFF (zlib) path, converting to raw SFNT bytes before handing to Blend2D; wire it into the font resolver's loadability check (`is_loadable_font_format`) so `format(woff2)`/`.woff2` sources stop being skipped, and load the decoded bytes via the on-disk font cache (or `createFromData`); Acceptance: an `@font-face` whose only src is a `.woff2` renders its glyphs instead of falling back; Tests: font decode unit tests + resolver integration. *(T-FONT-FACE-1 shipped @font-face parsing, family→src resolution, style-time `font_src`, and local + remote **raw TTF/OTF** loading; WOFF2 was deferred because Blend2D decodes only raw SFNT and no WOFF2 decompressor is bundled. The resolver already picks a raw-TTF `src` over a WOFF2 one when both are offered.)*

### Tech debt / refactors

- [ ] **[M10 P3] T-DOM-STYLE-COUPLING-1: DOM Node Should Not Point At Computed Style**; Goal: remove the inward-depends-outward edge `core/dom -> style/types`; Scope: `DOM::Node` caches a `shared_ptr<ComputedStyle>` for layout/paint convenience, so the innermost layer depends on the style layer (style is conceptually computed *from* the DOM, not vice versa). Consider a side table (node -> computed style) owned by the style/engine layer instead of a member on Node; Acceptance: `core/dom` no longer depends on `style/types`; Tests: style/layout tests unchanged. *(Filed 2026-07-17 from the T-ARCH-GUARD-2 package diagram. Pragmatic shortcut today; lower priority — the change touches every computed-style read site. Candidate M10 assigned 2026-07-17: do it after M7's DOM-surgery churn settles but before M12 multiplies computed-style read sites; reevaluate at M10 kickoff.)*

- [x] **[M7 P2] T-RESOURCE-TYPE-TABLE-1: Data-Drive Resource-Type Behavior** — DONE 2026-07-17 as part of story 7.0.1. One descriptor table in `ResourceRequestPlanner` (per-type request flags + decode mode) drives both the loader's request path and the update processor's ready path; `ProcessingStats`/`BatchResult` per-type bools became a `kResourceTypeCount`-sized ready array. `ResourceType::Script` then landed as one enum value + one table entry, proving the acceptance criterion. Table-consistency test added.; Goal: make adding a fetched resource type a one-place change instead of shotgun surgery; Scope: today a new `ResourceType` (Document/Stylesheet/Image/Font) threads its behavior mechanically across ~6 layers — the enum (`ResourceStore.h`), request options (`ResourceRequestPlanner`), the loader entry point (`ResourceLoader::request_*`), the update processor's per-type `handle_*` + `ProcessingStats`/`BatchResult` ready-flag, and the store. Introduce a per-type descriptor (e.g. `{binary?, decode-as, mark-ready-on-asset?, ready-flag}`) so the loader/processor dispatch off the table and a new type is mostly one descriptor entry; Acceptance: adding a hypothetical new type (e.g. Script/Media) touches ~1–2 files, not ~6; Tests: existing resource tests stay green; add one asserting the table drives dispatch. *(Filed 2026-07-16 while adding `ResourceType::Font` for T-FONT-FACE-1: the Font path was a clean mechanical mirror of Image at every layer — a sign of consistency, but also that the extension point is boilerplate-heavy. Not rot; pay down at the next natural touch point, e.g. when a 5th resource type lands.)*

### Form-control layout gaps

- [x] **T-FORM-FOCUS-RING-RADIUS-1: Synthetic Input Focus Ring Should Honor `border-radius`** — DONE 2026-07-18 (commit 3fe397e). `paint_input_focus_ring` now routes through `Layout::PaintUtils::draw_rounded_border_corners` (the rounded-stroke helper the CSS `outline` painter uses) with the input's resolved corner radii; the old four straight `fill_rect` edges are gone. Control-paint test asserts the ring draws but leaves the extreme corner unfilled. *(Filed + fixed 2026-07-18 from M7 todo-demo feedback.)* The broader unification is now tracked as T-FORM-FOCUS-UA-OUTLINE-1 below.

- [ ] **[M11 P3] T-FORM-FOCUS-UA-OUTLINE-1: Retire The Synthetic Focus Ring For A Real UA `:focus` Outline**; Goal: remove the special-case synthetic focus ring in `DocumentInputPainter` and instead give focusable controls a user-agent default `outline` on `:focus` (ideally `:focus-visible`), painted through the existing CSS `outline` path (`Layout::PaintUtils`); Scope: today `paint_input_focus_ring` hardcodes a blue ring for focused `<input>`s only (a stand-in from before `outline` was paintable). Now that `outline` is parsed and painted with radius support, fold the focus indicator into the real cascade: a UA stylesheet (or default rule) sets `:focus { outline: ... }`, `outline: none` from a page suppresses it, buttons/links/other focusables get it too, and `:focus-visible` distinguishes keyboard vs mouse focus; then delete the synthetic painter + `wants_synthetic_focus_ring`; Acceptance: focus indication comes entirely from CSS `outline` (page can override/remove it); no bespoke input-only ring remains; Tests: control paint + focus tests updated. *(Filed 2026-07-18 as the follow-up to T-FORM-FOCUS-RING-RADIUS-1, which made the existing ring rounded but left the mechanism bespoke. Needs `:focus-visible` support (not yet implemented) for full parity; lower priority. Related: T-FORM-INPUT-WIDTH-1.)*
- [ ] **[M11 P2] T-FORM-INPUT-WIDTH-1: Honor `width`/`box-sizing` On Inline-Block Inputs**; Goal: apply an explicit `width` (incl. percentages) and `box-sizing` to `<input>` controls in their default `display: inline-block`; Scope: today a bare `<input>` is laid out inline-block and takes an oversized default/intrinsic width, ignoring `width: 100%`, so it overflows its container (seen on the M7 `example.dev/todo` demo — worked around there with `display: block`, which routes the input through the block box-metrics path that *does* honor width); make the inline-block form-control path resolve `width`/`box-sizing` like the block path does; Acceptance: `input { width: 100% }` (inline-block) fits its containing block instead of overflowing; Tests: control layout tests. *(Filed 2026-07-18 building the M7 todo demo. Related: T-FORM-SELECT-1. Best fit M11 forms v2 with the other control-sizing work.)*

### DDG results-page gaps (found evaluating a live search 2026-07-16)

- [ ] **[M11 P1] T-FORM-SELECT-1: Native `<select>` Dropdown Control**; Goal: render and operate `<select>`/`<option>` controls (DDG results page: the "All Regions" / "Any Time" filters currently fall back to a generic element, leaking their option text as a smushed vertical column against the right edge); Scope: recognize `<select>`/`<option>` as controls, render a closed dropdown box showing the selected option, and (with forms v2 interaction) open/pick options; Scope note: `<option>` text must not render as flow content when closed; Acceptance: a `<select>` renders as a single control box with the selected label, not a stack of option texts; Tests: control layout + form value tests. *(Filed while evaluating the DDG results page; out of M6 scope — M6 form controls were bounded to the homepage's input+button. Best fit: M11 "Inputter" / forms v2.)*
- [ ] **[M10 P2] T-CSS-ABS-STATIC-1: Static Position For `top:auto`/`left:auto` Absolutes**; Goal: place an absolutely-positioned box with an `auto` inset at its **static position** (where it would sit in normal flow) rather than anchoring it to the containing block's top-left (DDG results page: the header logo `.header__logo-wrap{position:absolute; margin-top:-8px}` has `top:auto`, so it should sit at its flow position but we render it ~9px too high, vertically misaligned with the search box); Scope: compute/plumb the static position for absolute boxes and use it when the corresponding inset is auto; Acceptance: an `position:absolute; top:auto` box aligns to where it would be in flow, not y=0 of its containing block; Tests: positioning tests. *(Filed 2026-07-16 via F1 inspect: logo rect.y=56 vs the flow-aligned ~65. Real gap; non-trivial — proper static-position resolution.)*
- **T-UI-NAV-BACK-1** moved into Milestone 7 (`doc/milestones/milestone7.md`, story 7.6.1) on 2026-07-17.
- [ ] **[M10 P2] T-CSS-OBJECT-FIT-1: `object-fit` / `object-position` For Replaced Elements**; Goal: honor `object-fit` (`contain`/`cover`/`fill`/`none`) and `object-position` when sizing replaced elements like `<img>` (DDG results page: `.result__image__img { object-fit: cover }` on result thumbnails; currently unsupported so such images render at intrinsic size within their box); Scope: parse the properties + apply them in replaced-element sizing/paint; Acceptance: an `<img>` with a fixed box and `object-fit: contain`/`cover` scales within the box instead of overflowing; Tests: replaced-element layout tests. *(Filed 2026-07-16; standard properties, no prior story. The DDG header logo overflow initially looked like this but was actually a `background-size: auto <length>` bug, fixed in 98c15ec — object-fit remains a genuine gap for result thumbnails.)*

### Layouter v2 refinements (deferred from M6, flexbox non-goals)

- [ ] **[M10 P2] T-LAYOUT-FLEX-WRAP-COL-1: Column-Direction Flex Wrap**; Goal: support `flex-wrap` for `flex-direction: column` containers (multi-column), which M6 only models for rows; Scope: FlexBox line-breaking against a definite column main size (height) and placing wrapped lines as side-by-side columns, with the container cross size (width) growing to fit the columns; Acceptance: a column flex container with a fixed height and enough items wraps into multiple columns instead of one overflowing column; Tests: flex layout tests. *(Filed while completing T-DDG-LAYOUT-1; row wrap shipped, column wrap collapses to a single line.)*
- [ ] **[M10 P3] T-LAYOUT-FLEX-BASELINE-2: Accurate Flex Baseline From Nested Content**; Goal: derive a flex item's baseline from its first in-flow line box rather than approximating from the item's own font metrics; Scope: expose a first-baseline query on RenderObject/line boxes and have FlexBox `align-items: baseline` use it (also covers column cross-baseline); Acceptance: an item whose text lives in a nested child aligns on that child's baseline, not the item's synthesized font baseline; Tests: flex baseline tests. *(Filed while completing T-DDG-LAYOUT-1; current baseline uses the item's own font metrics, which is off for items with differently-styled nested content.)*
