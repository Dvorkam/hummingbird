# Adding A Native Form Control

Use this workflow for a small native control such as the M8 textarea MVP. Read
the active milestone first; a control must not silently absorb later Forms v2
semantics.

1. Add centralized tag/attribute names in `html/HtmlTagNames.h` and
   `html/HtmlAttributeNames.h`.
2. Extend `DocumentInputUtils` so focus, hit testing, and pseudo-state handling
   share the existing control predicates rather than adding a parallel path.
3. Add only the UA style/layout defaults the active story requires. Keep richer
   control sizing, scrolling, selection, and DOM APIs in their stated follow-up
   milestone.
4. Reuse `DocumentInputController` for editing and
   `DocumentInputPainter` for the native overlay. Any live, document-owned MVP
   value must be reset with the document; do not create a second owner for
   arena-backed nodes.
5. Update `FormSubmissionBuilder` so a successful control submits its current
   live value. Add a regression proving its exact encoded payload.
6. Add focused controller and tab/integration tests, then add or extend the
   relevant `assets/stub/pages/mN.html` demo and its “Added in MN” note.
7. Record every omitted browser behavior as a `T-*` story before landing the
   feature. Do not claim Forms v2 compatibility from an MVP control.

