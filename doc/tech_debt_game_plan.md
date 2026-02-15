# Tech Debt Game Plan (Temp)

## Chokepoint Candidates
- [x] **Engine::DocumentPipeline**: narrow the public API surface so Tab talks to smaller, purpose-built interfaces (render/layout vs interaction).
- [x] **Engine::Tab**: extract a `NavigationLifecycle` helper to own URL normalization, commit state, and security decisions (reduce coupling with `ResourceLoader`).
- [x] **Engine::Tab**: introduce a thin interaction facade so input/control forwarding no longer expands the Tab public surface.
- [ ] **App::BrowserApp**: split input routing into `ChromeEventRouter` vs `DocumentEventRouter` to separate UI chrome logic from document interaction.
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
