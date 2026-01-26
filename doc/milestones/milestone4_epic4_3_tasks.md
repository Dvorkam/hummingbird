# Milestone 4 — Epic 4.3 Task Breakdown

This checklist expands Epic 4.3 into trackable slices. Keep tasks small and cherry-pickable.

## Story 4.3.1 — Supported Feature Registry + Deduped Warnings

- [x] Create a central registry for supported HTML tags (one source of truth).
- [x] Create a central registry for supported CSS properties.
- [x] Emit unsupported tag warnings once per document.
- [x] Emit unsupported CSS property warnings once per document.
- [x] Add parser/style tests to prove deduped warnings.

## Story 4.3.2 — Selector Coverage

- [x] Implement universal selector (`*`) matching in selector matcher.
- [x] Implement descendant combinator matching (`.a .b`).
- [x] Implement compound selector matching (e.g., `div.hero`, `#id.class`).
- [x] Add selector matcher tests for each selector type.

## Story 4.3.3 — Decode Named Entities

- [x] Add a small whitelist of HTML named entities (`&mdash;`, `&nbsp;`, `&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`).
- [x] Decode entities in text nodes during HTML parsing.
- [x] Add parser tests that verify decoding in text content.

## Story 4.3.4 — Robust Parsing (HTML/CSS)

- [ ] HTML: tolerate malformed end tags (recover without aborting parse).
- [ ] HTML: tolerate unclosed tags with best-effort tree recovery.
- [ ] CSS: skip malformed declarations without stopping the rule.
- [ ] CSS: skip malformed rules without stopping the sheet.
- [ ] Add HTML/CSS parser tests for malformed input recovery.

## Story 4.3.5 — Text Readability

- [ ] Support `text-align` from CSS (not only legacy `align` attribute).
- [ ] Support `white-space: nowrap` from CSS.
- [ ] Ensure anchors are underlined by default (if not overridden).
- [ ] Support `em` units (font-relative lengths) in style parsing.
- [ ] Add style/layout tests for each readability feature.
