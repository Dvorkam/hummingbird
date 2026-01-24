# TODOs

## Milestone 3 (Navigator)

Milestone complete. See `doc/todo_archive/milestone3_done.md`.

## Milestone 4 (Scripting)

Milestone defined in `doc/milestones/milestone4.md` (stories moved there).

## Milestone 5 (Layout/Polish)

- [ ] **[M5 P1] T-CSS-POS-1: position:absolute (basic)**; Goal: support absolute positioning; Scope: layout flow; Acceptance: out-of-flow elements positioned by top/left/right/bottom; Tests: layout tests.
- [ ] **[M5 P1] T-CSS-VIS-1: opacity (paint-only)**; Goal: apply opacity in paint; Scope: Painter; Acceptance: subtree alpha scales paint; Tests: renderer tests.
- [ ] **[M5 P2] T-CSS-BOX-1: box-sizing**; Goal: support border-box; Scope: layout sizing; Acceptance: border-box affects width/height calc; Tests: layout tests.
- [ ] **[M5 P2] T-UI-FORM-1: Form Control Styling Polish**; Goal: native-like input/button visuals (shading, hover, pressed); Scope: renderer + style defaults; Acceptance: inputs/buttons look intentional and stateful; Tests: manual.
- [ ] **[M5 P2] T-CSS-BORDER-2: border-radius (paint)**; Goal: round corners; Scope: Painter; Acceptance: rounded rect paint for background/border; Tests: renderer tests.
- [ ] **[M5 P2] T-CSS-DECOR-1: outline + outline-offset**; Goal: draw outlines; Scope: Painter; Acceptance: outline draws outside border with offset; Tests: renderer tests.
- [ ] **[M5 P2] T-IMG-1: Animated GIF/WebP Playback**; Goal: play animated frames; Scope: decoder + renderer scheduling; Acceptance: frames render with timing; Tests: image tests.
- [ ] **[M5 P2] T-IMG-2: SVG Image Decode (Raster)**; Goal: rasterize SVG `<img>` sources; Scope: IImageDecoder + SVG library; Acceptance: svg renders to ImageBitmap; Tests: image tests.
- [ ] **[M5 P2] T-PERF-3: Split UI Chrome From Page Render**; Goal: avoid repainting page while editing URL bar; Scope: app render split; Acceptance: URL bar updates without page repaint; Tests: manual.

## Milestone 6+ (Big Rocks)

- [ ] **[M6 P1] T-PERF-4: Offscreen Raster Cache + Layer Invalidation**; Goal: repaint only dirty regions; Scope: renderer + engine invalidation; Acceptance: cached layers reused across frames; Tests: renderer perf tests.
- [ ] **[M6 P1] T-CACHE-1: Tab Resource Eviction + Rehydrate**; Goal: evict resources/render tree for background tabs and restore on focus; Scope: Tab + ResourceStore; Acceptance: inactive tabs drop memory and reload on activation; Tests: engine tests.
- [ ] **[M6 P1] T-DOM-1: Infinite Scroll DOM Virtualization**; Goal: cap live DOM/resources for unbounded feeds; Scope: DOM/layout + resource eviction; Acceptance: long feeds do not grow memory unbounded and can rehydrate when revisiting content; Tests: engine perf tests.
- [ ] **[M6 P1] T-DOM-2: DOM Budget Failure UX**; Goal: show a stable error page and recovery action when DOM budget is exceeded; Scope: DocumentPipeline + ResourceLoader; Acceptance: budget failure shows user-facing page instead of blank reset; Tests: engine tests.
- [ ] **[M6 P1] T-PERF-5: Batch Resource Updates**; Goal: coalesce resource arrivals into fewer style/layout passes; Scope: ResourceLoader + DocumentPipeline; Acceptance: heavy pages avoid repeated full rebuilds; Tests: engine perf tests.
- [ ] **[M6 P1] T-LAYOUT-FLEX-1: Flexbox Layout MVP**; Goal: basic flex layout; Scope: layout engine; Acceptance: common flex rows/columns render; Tests: layout tests.
- [ ] **[M6 P2] T-LAYOUT-GRID-1: Grid Layout MVP**; Goal: minimal CSS Grid support; Scope: layout engine; Acceptance: fixed-track grids render; Tests: layout tests.
- [ ] **[M6 P2] T-ANIM-1: transition + transform (static)**; Goal: parse/apply transforms without timing engine; Scope: style + paint; Acceptance: transform affects paint matrix; Tests: renderer tests.
