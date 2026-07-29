> **Status: Planned** — created 2026-07-26 by triaging `doc/TODOs.md` at the M9
> kickoff. Stories were moved here from the backlog; **revalidate every Scope line
> against the codebase at kickoff.**

## Milestone 11 North Star Deliverable

**Before (after Milestone 10):**

* Forms work only as far as each proof target forced them. `<textarea>` is M8's
  deliberately narrow HN-comment MVP: typing, Backspace, and Enter-as-newline, with
  its value folded into a `value` **attribute** so it could reuse `<input>`'s
  single-owner value path. There is no selection, no word/line navigation, no
  clipboard, no `selectionStart`, no `defaultValue`, no form reset.
* `<input type=password>` renders its value as **plain readable glyphs**.
* You cannot paste into a focused document text control — the URL bar can, the page
  cannot.
* `<select>` is not a control at all: its `<option>` text leaks into flow content.
* Focus indication is a bespoke synthetic blue ring painted only for `<input>`,
  predating the CSS `outline` painter that now exists.

**After (Milestone 11 done):**

* **Form Controls v2:** the full textarea model (value/defaultValue, selection,
  editing, sizing, internal scrolling) and a real `<select>`.
* **Focus System:** tab order, a UA `:focus`/`:focus-visible` outline through the
  real cascade, keyboard routing.
* **Selection + Clipboard:** copy/cut/paste across controls and document text.
* **Text rendering reality check:** emoji + font-fallback baseline (comment sections
  are full of them; full shaping/bidi stays deferred).
* **Proof target:** **log into a major site (GitHub or Wikipedia) reliably**; write,
  select, copy/paste, and edit multi-line text without glitches.

*Continuity note:* M8 already proved a real login end-to-end (Hacker News), but did
it the hard way — the user could not paste a generated password and watched it
render in plain text. This milestone is what makes that flow *comfortable* rather
than merely possible.

---

## Non-Goals

* No IME/composition events — Latin input "good enough" first; composition is a
  follow-up epic behind interfaces.
* No full text shaping or bidi. Emoji + fallback chain only.
* No rich-text/contenteditable editor model.
* No accessibility tree / screen-reader surface (not yet scheduled anywhere — flag
  at kickoff if a proof target forces it).

---

## Critical Path (what must land for the proof target)

**Must-have**

* Clipboard paste into focused controls (11.1.1) — a login flow that cannot accept a
  pasted password is not a usable login flow.
* Password masking (11.1.2) — shipping a browser that displays typed passwords is
  not defensible past this point.
* Textarea selection/navigation/editing (11.2.1) and its DOM/form semantics
  (11.2.2) — together these retire M8's documented deviation.
* Focus system + tab order (11.3.1) — keyboard-only form completion.

**Foundational**

* 11.2.2 must land before or with 11.2.1: the value model decision (live value vs
  default value) determines what editing operates on. M8's
  `doc/dev_guide/form_control_workflow.md` already leads with "decide where the value
  lives first" — that lesson applies directly here.

---

## Milestone 11 Done When

* A real login on GitHub or Wikipedia completes using only the keyboard, with a
  pasted password, rendered masked.
* A multi-line comment can be typed, selected by mouse and keyboard, cut, pasted,
  and edited without corrupting the value, and a form reset restores defaults.
* `<textarea>` no longer exposes a `value` attribute to JS, and the child text node a
  page expects is present.
* `<select>` renders as one control box showing the selected label.
* Focus indication comes entirely from CSS `outline`; no bespoke input-only ring
  remains.

---

## Stories

### 11.1 - Immediate Form Gaps (carried from the M8 HN login)

* **Story 11.1.1: Paste Into Focused Text Controls (T-FORM-INPUT-PASTE-1)** [P1]
* **Goal:** let `Ctrl+V` paste clipboard text into a focused `<input>`/`<textarea>`.
* **Scope:** the URL bar already pastes (`UrlBarTest.PasteUsesClipboardText`, via the
  platform clipboard), but a focused document text control ignores `Ctrl+V` — so a
  generated password had to be retyped by hand. Route a paste in
  `DocumentInputController` through the same platform clipboard read the URL bar
  uses, inserting at the caret (multi-line paste splits on newlines for a textarea,
  is flattened for an input).
* **Acceptance:** `Ctrl+V` into a focused input/textarea inserts the clipboard text
  at the caret and the value submits correctly.
* **Tests:** input-controller paste tests.
* *Filed 2026-07-22 from the HN login. Small and high-value; a precursor to the full
  copy/cut/paste + selection work in 11.2.1. **Was tagged M9; moved here at the M9
  kickoff** because it belongs with the clipboard system rather than the networking
  milestone — but it is small enough to pull forward if it blocks a demo.*

* **Story 11.1.2: Mask `<input type=password>` Text (T-FORM-PASSWORD-MASK-1)** [P1]
* **Goal:** stop rendering a password field's value as plain readable glyphs.
* **Scope:** `input[type=password]` currently paints its value verbatim — visible
  while typing, seen logging into HN. Render each character as a mask glyph
  (U+2022 •) in `DocumentInputPainter` while keeping the real value in the model for
  submission, with caret math measuring the masked run so the caret still tracks. The
  brief last-char reveal browsers do on mobile is out of scope.
* **Acceptance:** typing into a password field shows bullets, and the form still
  submits the real characters.
* **Tests:** input paint + submission tests.
* *Filed 2026-07-22 from the HN login — the user flagged visible password text as
  "fine for now but needs a story soonish". **Was tagged M9; moved here at the M9
  kickoff.** P1 because it is a shoulder-surfing gap on a control people type secrets
  into; pull it forward if any demo involves a login.*

### 11.2 - Textarea and Forms v2

* **Story 11.2.1: Textarea Selection, Navigation, and Editing Semantics
  (T-FORM-TEXTAREA-EDITING-1)** [P1]
* **Goal:** grow M8 story 8.0.1's HN-only insert/Backspace/newline control into an
  editable multiline text control.
* **Scope:** caret movement by character/word/line, Home/End, Shift-based mouse and
  keyboard selection, delete, undo boundary decisions, and copy/cut/paste routed
  through this milestone's focus/clipboard system.
* **Acceptance:** users can select and replace arbitrary multiline ranges without
  corrupting the value.
* **Tests:** input-controller editing/selection tests.

* **Story 11.2.2: Textarea DOM and Full Form Semantics
  (T-FORM-TEXTAREA-API-1)** [P1]
* **Goal:** make `<textarea>` conform to the DOM/form behavior pages expect.
* **Scope:** `.value`/`.defaultValue`, `selectionStart`/`selectionEnd`,
  `setRangeText`, `disabled`/`readOnly`, form reset/default-value behavior, and
  normalized textarea line-ending serialization.
* **Acceptance:** JS and native editing observe one coherent live/default value
  model, and a reset restores the default.
* **Tests:** script-host + form-submission tests.
* **Known M8 deviation this story must unwind:** M8 folds a textarea's parsed
  content into a `value` **attribute** so the control could reuse `<input>`'s
  single-owner value path. Real browsers have no such attribute, so
  `getAttribute('value')` and `innerHTML` serialization both observe it, and the
  child text node a page might expect is absent. Replacing it with a proper
  live/default value pair is this story's job. *This is the load-bearing decision for
  11.2.1 — sequence it first.*

* **Story 11.2.3: Textarea Sizing, Internal Scrolling, and CSS `resize`
  (T-FORM-TEXTAREA-LAYOUT-1)** [P2]
* **Goal:** complete textarea presentation beyond M8's fixed multiline box.
* **Scope:** `rows`/`cols` sizing semantics, content overflow and internal scroll
  offsets, scrollbar/hit-testing behavior, and CSS `resize`.
* **Acceptance:** a long value remains editable and visible through an independently
  scrolling/resizable control.
* **Tests:** control layout + interaction tests.
* **Known M8 gaps to close here:** `cols` is ignored (fixed default width); the UA
  reserves rows using the **parent's** font size, because UA defaults run before
  inheritance (`StyleEngine::build_style_for` applies
  `apply_user_agent_defaults` before `apply_properties_to_style` and before
  `inherit_from`), so a `font-size`/`line-height` set on the textarea itself
  under-reserves; and lines past the bottom edge are simply not painted rather than
  scrolled to.
* **Depends on M10's 10.2.1** (real scroll containers) for the internal-scroll half.

* **Story 11.2.4: Native `<select>` Dropdown Control (T-FORM-SELECT-1)** [P1]
* **Goal:** render and operate `<select>`/`<option>` controls.
* **Scope:** recognize `<select>`/`<option>` as controls, render a closed dropdown
  box showing the selected option, and (with forms v2 interaction) open and pick
  options. `<option>` text must not render as flow content when closed.
* **Acceptance:** a `<select>` renders as a single control box with the selected
  label, not a stack of option texts.
* **Tests:** control layout + form value tests.
* *Filed while evaluating the DDG results page, where the "All Regions" / "Any Time"
  filters leak their option text as a smushed vertical column against the right
  edge.*

* **Story 11.2.5: CSS-Customizable Controls (`appearance`, `:checked`,
  `accent-color`) (T-FORM-CONTROL-CSS-1)** [P2]
* **Goal:** let pages restyle form controls instead of only getting the hardcoded
  native look.
* **Scope:** the 7.2.6 checkbox is painted as a fixed native box+checkmark in
  `DocumentInputPainter` and cannot be styled. Real customization needs four pieces:
  (1) `appearance: none` to opt out of native rendering (currently warns
  "unsupported"); (2) a `:checked` pseudo-class in the selector engine (today only
  `:hover`/`:active`/`:focus`/`:visited` exist); (3) `::before`/`::after`
  pseudo-elements (common for custom check glyphs); (4) honoring CSS box properties
  on the control. A lighter partial win is `accent-color` to recolor the native
  control.
* **Acceptance:** `input[type=checkbox]{ appearance:none; … }` + `:checked` renders a
  custom checkbox; at minimum `accent-color` recolors the native one.
* **Tests:** selector + control paint tests.
* **Depends on M10's 10.5.1** — adding `:checked` and `::before`/`::after` to the
  selector engine is exactly the work that must not reintroduce the
  silently-widening-selector bug. Do the validity model first, then add these as
  *supported* pseudos.

* **Story 11.2.6: Honor `width`/`box-sizing` On Inline-Block Inputs
  (T-FORM-INPUT-WIDTH-1)** [P2]
* **Goal:** apply an explicit `width` (including percentages) and `box-sizing` to
  `<input>` controls in their default `display: inline-block`.
* **Scope:** a bare `<input>` is laid out inline-block and takes an oversized
  default/intrinsic width, ignoring `width: 100%`, so it overflows its container
  (seen on the M7 `example.dev/todo` demo — worked around there with
  `display: block`, which routes the input through the block box-metrics path that
  *does* honor width). Make the inline-block form-control path resolve
  `width`/`box-sizing` like the block path.
* **Acceptance:** `input { width: 100% }` (inline-block) fits its containing block.
* **Tests:** control layout tests.
* **Related to M10's 10.4.3** — both are "the inline path lacks the containing-block
  context the block path has." Check whether 10.4.3's plumbing solves this for free.

### 11.3 - Focus System

* **Story 11.3.1: Tab Order and Keyboard Routing** *(new story — roadmap headline)*
* **Goal:** Tab/Shift+Tab move focus through focusable elements in document order,
  and keyboard events route to the focused control.
* **Scope:** focus ring/order model over the existing focus state; interaction with
  chrome focus (URL bar) so Tab does not escape the document unexpectedly.
* **Acceptance:** a login form can be completed entirely from the keyboard.
* **Tests:** focus-order + key-routing tests.

* **Story 11.3.2: Retire The Synthetic Focus Ring For A Real UA `:focus` Outline
  (T-FORM-FOCUS-UA-OUTLINE-1)** [P3]
* **Goal:** remove the special-case synthetic focus ring in `DocumentInputPainter`
  and give focusable elements a UA default `outline` on `:focus` (ideally
  `:focus-visible`), painted through the existing CSS `outline` path.
* **Scope:** `paint_input_focus_ring` hardcodes a blue ring for focused `<input>`s
  only — a stand-in from before `outline` was paintable. Now that `outline` is parsed
  and painted with radius support (`T-FORM-FOCUS-RING-RADIUS-1`, done 2026-07-18),
  fold the focus indicator into the real cascade: a UA rule sets
  `:focus { outline: … }`, a page's `outline: none` suppresses it, buttons/links/other
  focusables get it too, and `:focus-visible` distinguishes keyboard from mouse
  focus; then delete the synthetic painter and `wants_synthetic_focus_ring`.
* **Acceptance:** focus indication comes entirely from CSS `outline` (the page can
  override or remove it); no bespoke input-only ring remains.
* **Tests:** control paint + focus tests updated.
* **Needs `:focus-visible`**, which does not exist — and therefore **depends on
  M10's 10.5.1** for the same reason 11.2.5 does.

* **Story 11.3.3: Focus/Blur Listener Mutations Not Synced To Layout
  (T-FOCUS-MUTATION-SYNC-1)** [P2]
* **Goal:** make every call site that can fire `blur`/`focus`/`change` react to a
  JS mutation the way the click-focus path already does.
* **Scope:** story 7.7.2 made `focus_input_at`/`focus_autofocus_input`/
  `clear_input_focus` fire real JS-visible focus events (capable of mutating the
  DOM), but `fire_focus_transition`'s own `mutated` return value is discarded by all
  three wrappers, which return only whether focus *state* changed.
  `DocumentEventRouter`'s click-focus path is safe because it unconditionally calls
  `refresh_styles_for_interaction()` whenever focus/interaction state changes at all —
  but two call sites do not: `ChromeEventRouter::handle_global_key_shortcut`'s
  `Ctrl+L` handler (`clear_input_focus()`, only sets a paint-only dirty flag) and
  `Tab::apply_autofocus_after_rebuild` (`focus_autofocus_input()`, only calls
  `mark_dirty()`).
* **Acceptance:** a `blur`/`change` listener that mutates the DOM on `Ctrl+L`, or a
  `focus` listener that mutates on autofocus, is reflected in the next paint without
  waiting for an unrelated rebuild.
* **Tests:** engine/Tab + ChromeEventRouter tests.
* *Filed 2026-07-20 from the pre-PR M7 review; **was tagged M8, re-homed here at the
  M9 kickoff** — it is a focus-system bug and this is the focus-system milestone.
  Currently latent: no demo page wires a focus listener that mutates the DOM.*

### 11.4 - Text Rendering Reality Check

* **Story 11.4.1: Emoji + Font-Fallback Chain Baseline** *(new story — roadmap
  headline)*
* **Goal:** render emoji and fall back through the font chain instead of dropping to
  a single family.
* **Scope:** font-fallback chain in the text pipeline; colour emoji rendering.
  Comment sections — the thing this milestone exists to let users write in — are full
  of them.
* **Acceptance:** a comment containing emoji and mixed scripts renders without
  tofu boxes for the covered ranges.
* **Tests:** text shaping/fallback tests + glyph telemetry.

* **Story 11.4.2: `word-break` / `overflow-wrap` / `line-clamp` / `hyphens`
  (T-CSS-TEXT-WRAP-2)** [P3]
* **Goal:** control how long or overflowing text breaks and truncates.
* **Scope:** all warn "unsupported" and fall back to default line breaking. The
  visible one for portals is `line-clamp` (truncate a teaser to N lines with an
  ellipsis — seznam/novinky card summaries rely on it and currently render
  full-length, shoving layout), while `word-break`/`overflow-wrap` (already flagged
  cosmetic on HN) prevent long tokens from overflowing. Add them to the
  inline/line-breaking path.
* **Acceptance:** a `-webkit-line-clamp:3` block truncates to three lines with an
  ellipsis; a long unbreakable token with `overflow-wrap:break-word` wraps instead of
  overflowing.
* **Tests:** inline/line-box tests.
* *Best fit here because this milestone already owns the line-box machinery for emoji
  and fallback. Note `line-clamp` is the correct answer to the M8.5 observation that
  `overflow:hidden` clips mid-line — clean truncation is line-clamp's job, not
  overflow's.*

---

## Execution Order Checklist

Foundational
- [ ] 11.2.2: Textarea DOM and Full Form Semantics *(decides the value model 11.2.1 edits)*

P0: Proof target (reliable login + comfortable text entry)
- [ ] 11.1.1: Paste Into Focused Text Controls
- [ ] 11.1.2: Mask `<input type=password>` Text
- [ ] 11.3.1: Tab Order and Keyboard Routing
- [ ] 11.2.1: Textarea Selection, Navigation, and Editing
- [ ] 11.2.4: Native `<select>` Dropdown Control

P1: Presentation + text reality
- [ ] 11.2.3: Textarea Sizing and Internal Scrolling *(needs M10 10.2.1)*
- [ ] 11.4.1: Emoji + Font-Fallback Chain Baseline
- [ ] 11.2.6: Honor `width`/`box-sizing` On Inline-Block Inputs
- [ ] 11.3.3: Focus/Blur Listener Mutations Synced To Layout

P2/P3: If schedule allows
- [ ] 11.2.5: CSS-Customizable Controls *(needs M10 10.5.1)*
- [ ] 11.3.2: Retire The Synthetic Focus Ring *(needs `:focus-visible`, so needs M10 10.5.1)*
- [ ] 11.4.2: `word-break` / `overflow-wrap` / `line-clamp` / `hyphens`
