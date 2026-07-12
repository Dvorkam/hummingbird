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
