# Adding A Native Form Control

Use this workflow for a small native control such as the M8 textarea MVP. Read
the active milestone first; a control must not silently absorb later Forms v2
semantics.

1. **Decide where the control's value lives before writing anything else.** This
   is the decision the rest of the work falls out of. Prefer normalizing whatever
   the markup supplies into the one place the existing controls already read —
   the `value` attribute — at parse time. `<textarea>` does this: its element
   content is folded into `value` by the parser, which is why focus, editing,
   painting, and submission all reuse the `<input>` path untouched, and why no
   stray text node renders beneath the overlay. A control that sources its value
   from anywhere else forces a parallel path through every one of those stages.
2. If the control's content is not ordinary markup, teach the tokenizer next.
   `HtmlTagMetadata` distinguishes raw text (literal, character references left
   alone — script/style) from escapable raw text (markup literal, references
   decoded — textarea). Getting this wrong silently corrupts any value
   containing `<` or `&`.
3. Add centralized tag/attribute names in `html/HtmlTagNames.h` and
   `html/HtmlAttributeNames.h`.
4. Extend `DocumentInputUtils` so focus, hit testing, and pseudo-state handling
   share the existing control predicates rather than adding a parallel path.
   Keep layout, style, and engine agreeing on *one* tag list:
   `TagMetadata::is_text_control_tag` serves all three layers so hardcoded
   `tag == Input || tag == Textarea` checks cannot drift apart.
5. Add only the UA style/layout defaults the active story requires. Keep richer
   control sizing, scrolling, selection, and DOM APIs in their stated follow-up
   milestone. Where layout reserves space per line, the painter must advance by
   the *same* computed value — a UA constant that only layout knows will not
   survive a font change.
6. Reuse `DocumentInputController` for editing and `DocumentInputPainter` for the
   native overlay. Any live, document-owned MVP value must be reset with the
   document; do not create a second owner for arena-backed nodes.
7. Update `FormSubmissionBuilder` so a successful control submits its current
   live value. Add a regression proving its exact encoded payload.
8. Add focused controller and tab/integration tests, then add or extend the
   relevant `assets/stub/pages/mN.html` demo and its “Added in MN” note. Exercise
   a **prefilled** control, not just an empty one — an empty control hides every
   defect in the value-sourcing path.
9. Record every omitted browser behavior, and every deviation the MVP's value
   model introduces, as a `T-*` story before landing the feature. Do not claim
   Forms v2 compatibility from an MVP control.

