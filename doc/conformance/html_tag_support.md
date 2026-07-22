# HTML Tag Support

What the parser and UA stylesheet actually do with element tags, and which tags
we recognize versus render blindly. Companion to the CSS side (the property
registry — see the end of this file). Work items live in `doc/TODOs.md`; this
file is the adherence picture and links to `T-*` ids.

## The three tiers a tag can be in

1. **Recognized + presented.** The tag is in `TagMetadata::is_supported_tag`
   (`src/html/HtmlTagMetadata.h`) *and* `StyleDefaults::apply_user_agent_defaults`
   gives it its intended box/type/margins (block vs inline, list markers,
   headings, table parts, form controls, `<a>` underline+color, `<pre>`
   monospace, …). This is the tag working as intended.
2. **Recognized structurally only.** In the known-tag list so it does not warn,
   but with no special UA styling — it inherits the generic default
   (`display:block`). The semantic sectioning tags (`<header>`/`<nav>`/`<main>`/
   `<section>`/`<article>`/`<aside>`/`<footer>`) are here: they map to a block
   and nothing more (logged once at DEBUG).
3. **Unrecognized.** Not in the known-tag list. It is **still parsed into the
   DOM and still rendered** — as a generic `display:block` box — but logs
   `Unsupported HTML Tag encountered: <x>` once per tag name, and gets none of
   the tag's presentation. Custom elements (any name with a `-`) are exempt from
   the warning by design.

The important consequence of tier 3: an unknown tag is never *dropped*, but a
tag whose whole job is presentational (centering, underline, an inline wrapper)
renders **wrong**, not absent — and if the tag is normally inline, defaulting it
to block also breaks the surrounding text flow.

## Known-unsupported tags seen on real proof targets

| Tag | Should be | Today (tier 3) | Impact | Ticket |
|---|---|---|---|---|
| `<center>` | block that centers its contents (legacy `align:center`) | generic block, contents left-aligned | HN wraps its whole `#hnmain` layout in `<center>`; the page renders left-hugging instead of centered | [T-HTML-PRESENTATIONAL-TAGS-1](../TODOs.md) |
| `<u>` | **inline**, underlined | generic **block** (not in the inline list), no underline | breaks inline text flow wherever HN uses it inline, on top of missing the underline | [T-HTML-PRESENTATIONAL-TAGS-1](../TODOs.md) |

These are the two the Hacker News item page actually hits. Other legacy
presentational tags (`<s>`, `<strike>`, `<small>`, `<big>`, `<tt>`, `<sub>`,
`<sup>`, `<mark>`) are the same shape of gap and should be closed in the same
pass when a proof target needs them — add a row when one does.

## How to close a tag gap

1. If the tag needs a name constant, add it to `HtmlTagNames.h`.
2. Add it to `kKnownTags` in `HtmlTagMetadata.h` so it stops warning.
3. Give it its box/type in `StyleDefaults::apply_user_agent_defaults`
   (inline vs block, and any margins/decoration). For `<center>` that is
   `display:block; text-align:center`; for `<u>` it is `display:inline` plus an
   underline like the `<a>` branch already sets.

Prefer routing presentational effects through the same `ComputedStyle` fields
real CSS would set (e.g. `text_align`, `underline`), not a bespoke path, so a
page's own CSS still overrides them by the normal cascade.

## CSS property support

Property recognition is data-driven in the CSS property registry
(`src/style/registry/CssPropertyList.inl`); an unrecognized declaration logs
`Unsupported CSS property encountered: <name>` once and is skipped (the rest of
the rule still applies). Properties observed unsupported on the HN item page,
all cosmetic for that page:

| Property | Effect of absence | Notes |
|---|---|---|
| `resize` | textarea has no drag-resize handle | tracked for the textarea control by [T-FORM-TEXTAREA-LAYOUT-1](../TODOs.md) |
| `word-break` / `overflow-wrap` | long unbroken tokens (URLs) can overflow their box instead of breaking | line-breaking refinement; no proof target forces it yet |
| `page-break-before` | print-only; no effect on screen layout | out of scope until a print/paginated target exists |

Add a row here when a real page depends on a property we skip; schedule it with
a `T-*` ticket only when a proof target needs it, per the demo-driven strategy.
