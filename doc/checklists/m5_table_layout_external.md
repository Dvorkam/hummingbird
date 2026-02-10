# M5 Table Layout External Verification Checklist

Purpose: manual pass for `T-TABLE-LAYOUT-4` against representative pages/demos.

## Setup

1. Build and run the browser.
2. Open with debug outlines off.
3. Keep viewport in a normal desktop width range first, then retest narrower.

## Internal Demo (StubNetwork)

URL: `https://example.dev`

1. Scroll to `Tables`.
2. Verify `50/50 width-hint balancing demo` keeps both columns readable.
3. Verify `Overcommitted width-hint demo` does not collapse one column to near-zero.
4. Verify `Absolute width-hint demo` keeps first column visually fixed and second starts exactly at the seam.
5. Verify `Real-page-like table demo`:
6. Header and body column seams align.
7. No doubled middle border line.
8. No row overlap or horizontal text clipping.

## External-Like Page Spot Checks

Use at least one content-heavy page and one form-heavy page with table usage.

1. Confirm table text remains readable at first load.
2. Confirm there is no severe column collapse on first paint.
3. Confirm scrolling does not introduce doubled seam lines.
4. Confirm form controls near tables remain interactive and aligned.

## Pass/Fail

Mark pass only if all checks above succeed without severe collapse or seam artifacts.
