# Tech Debt Game Plan

Tracked refactoring seams. When touching a file listed here, prefer the planned
decomposition over ad-hoc changes; check items off (with a note) when they land or
are superseded.

## Chokepoint Candidates
- [x] **Engine::DocumentPipeline**: narrow the public API surface so Tab talks to smaller, purpose-built interfaces (render/layout vs interaction). *(Superseded: the `LayoutApi`/`InteractionApi` facades were pure pass-throughs that tripled each method declaration; flattened back to a single grouped public API.)*
- [x] **Engine::Tab**: extract a `NavigationLifecycle` helper to own URL normalization, commit state, and security decisions (reduce coupling with `ResourceLoader`).
- [x] **Engine::Tab**: introduce a thin interaction facade so input/control forwarding no longer expands the Tab public surface. *(Superseded: same pass-through pattern; flattened into public interaction methods on Tab.)*
- [x] **App::BrowserApp**: split input routing into `ChromeEventRouter` vs `DocumentEventRouter` to separate UI chrome logic from document interaction.
- [x] **App::BrowserApp**: remove `friend` access for routers/`RenderCoordinator`. Routers now take explicit collaborators (`BrowserChrome`, `TabController`, `RenderCoordinator`, window/graphics); event dispatch glue lives in `BrowserEventRouter`; content-viewport geometry moved to `BrowserChrome::content_viewport`; debug outlines and dirty-flag combos moved to `RenderCoordinator`; ensure-active-tab invariant moved to `TabController::ensure_active_tab`.
- [ ] **App::BrowserApp**: move tab lifecycle actions (`new_tab`, `close_active_tab`, activate next/prev) and active-tab change side effects into `TabController` with a small result object for UI updates.
- [ ] **App::BrowserApp**: move URL bar focus/blur and security-state sync into `BrowserChrome` helpers (e.g., `focus_url_bar(window)`, `sync_security_state(state)`).
- [ ] **Layout::RenderTable**: isolate seam debug logging into a debug-only helper or compile-time gated block.
- [ ] **Layout::RenderTable**: consolidate row/column hint passes into a `TableMetrics` helper.
- [ ] **Platform API surface**: split `InputEvent` variants into smaller headers/PODs to reduce include fan-out.
- [ ] **Platform API surface**: move platform factory usage behind cpp-local includes and forward declarations in app/engine headers.

## Large File Candidates (Remaining)
- [ ] `src/style/parser/CssParser.cpp`: extract property-specific parsing helpers (font/background/border) to reduce `parse_value` branching.
- [ ] `src/style/parser/CssParser.cpp`: deduplicate tokenization/parse patterns used across shorthand handlers.
- [ ] `src/layout/flow/TextBox.cpp`: separate text shaping/metrics from line-breaking logic.
- [x] `src/layout/flow/TextBox.cpp`: isolate text-overflow/ellipsis handling into a helper.
- [x] `src/layout/flow/TextBox.cpp`: consolidate text measurement into a reusable `TextMeasurer` helper.
- [ ] `src/platform/graphics/SDLGraphicsContext.cpp`: split rendering primitives (rects/text/images) vs setup/teardown.
- [ ] `src/platform/graphics/SDLGraphicsContext.cpp`: isolate font cache/text rendering into a helper.
- [ ] `src/platform/graphics/SDLGraphicsContext.cpp`: extract image caching/eviction into an `ImageCache` helper.
- [ ] `src/style/compute/apply/ApplyLayout.cpp`: split property application by domain (layout, borders, spacing).
- [ ] `src/style/compute/apply/ApplyLayout.cpp`: centralize percent/auto length handling into shared helpers.
- [ ] `src/engine/tab/Tab.cpp`: separate navigation lifecycle, extension CSS injection, and input handling responsibilities further.
- [ ] `src/engine/tab/Tab.cpp`: reduce shared mutable state across tick/update paths.
- [ ] `src/engine/document/DocumentInputController.cpp`: extract focus/interaction state machine.
- [ ] `src/engine/document/DocumentInputController.cpp`: move hit-testing responsibilities into `DocumentInteraction` where possible.
- [ ] `src/platform/net/CurlNetwork.cpp`: extract a `CurlRequest` helper to unify GET/POST setup and TLS policy.
- [ ] `src/engine/extensions/ExtensionManifest.cpp`: split parsing, validation, and error reporting.
- [ ] `src/engine/extensions/ExtensionManifest.cpp`: extract a small `JsonMiniParser` (cursor/skip helpers) to reduce bespoke parsing code.
- [ ] `src/engine/resources/ResourceLoader.cpp`: extract a `DocumentFetchPolicy` helper to separate navigation policy from request handling.
- [ ] `src/engine/document/DocumentPipeline.cpp`: keep the orchestrator thin by pushing any new business logic into helpers.

## CSS Property System Ceremony

Surfaced while adding `clear` (T-CSS-CLEAR-1): a genuinely new property with its
own field touches ~5 wiring points before the actual behavior. The layering is
correct (Ports & Adapters firewall holds), but two seams are avoidable ceremony.

- [ ] **T-STYLE-FIELDCOPY-1: Make `apply_non_inheritable` drift-proof** (`src/style/compute/StyleEngine.cpp`). Goal: a non-inherited `ComputedStyle` field that is added but not hand-copied is a *silent* runtime bug (hit repeatedly — flex fields, `clear`); replace the hand-enumerated copy with a mechanism that cannot drift (single memberwise copy of a non-inherited sub-struct, or a compile-time/reflection check that every non-inherited field is covered). Acceptance: forgetting to wire a new non-inherited field fails at build/test time, not silently at runtime. **Highest-value item here** — it is the one actually-fragile seam, not just verbose.
- [ ] `src/style/registry` + `src/style/compute/apply/PropertyApplier.cpp`: collapse the `ApplyHook` enum + dispatch `switch` redundancy. The registry `.inl` already names the applier; a hand-maintained enum plus a switch that only forwards to a function is boilerplate. Consider keying appliers directly off the registry (function pointer / small dispatch table) so adding a simple property needs one registry row + one apply case, not four synced touchpoints.
