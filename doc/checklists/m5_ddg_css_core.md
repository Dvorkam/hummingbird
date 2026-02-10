# M5 DDG CSS Core Checklist

Story: `T-CSS-DDG-CORE-1`

Goal: verify the high-impact DDG CSS compatibility slice is present and keeps the DDG search controls readable/usable.

## Scope Mapping

- `font` shorthand: parser/style tests present (`tests/style/CSSParser.test.cpp`, `tests/style/StyleEngine.test.cpp`)
- `border-top/right/bottom/left` shorthands: parser/style tests present
- `border-radius` shorthand: style/paint tests present
- `overflow` and `overflow-y`: parser/style tests present
- `vertical-align`: parser/style and layout tests present
- `cursor`: parser/style tests present
- `text-indent`: style tests present
- `box-shadow`: parser/style tests present

## Manual Verification Steps

1. Launch app against stub network page (`https://example.dev`).
2. In **Typography & Inline Elements** section:
- Confirm `vertical-align` chips (`top`, `middle`, `bottom`) visibly differ from baseline.
3. In **Border Styles** section:
- Confirm rounded corners on `border-radius` demo.
- Confirm side-specific border widths/styles appear on `border-sides-demo`.
- Confirm `box-shadow` is visible on `box-shadow-demo`.
4. In **Text Readability (CSS)** section:
- Confirm `text-indent` pushes first line right.
5. In **Fonts & Czech Characters** section:
- Confirm `font-demo-shorthand` text renders with larger size and italic/bold style.
6. Open DDG HTML page and verify search controls:
- Input and submit control are readable.
- Input and submit control do not overlap.
- Search query can be typed and submitted.

## Pass/Fail Log

- Date:
- Environment:
- Result: `PASS` / `FAIL`
- Notes:
