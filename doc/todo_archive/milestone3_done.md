# Milestone 3 Done

- [x] **[M3 P2] T-BRAND-1: Windows ICO Sizes**; Goal: add 48/64/128 sizes to ico; Scope: `assets/icons/hummingbird.ico`; Acceptance: Windows scaling looks correct at common sizes; Tests: n/a.
- [x] **[M3 P2] T-BRAND-2: Runtime Icon Layout**; Goal: split runtime PNG/ICO into `assets/icons/` and keep SVGs in `assets/logos/`; Scope: asset moves + references; Acceptance: app loads icons from new paths; Tests: `ctest --preset user-ninja-multi-vcpkglt`.
- [x] **[M3 P2] T-DOC-1: Update Docs/README After Refactor**; Goal: document current structure and build/run notes; Scope: `README.md` + relevant docs; Acceptance: docs match current code layout and workflows; Tests: n/a.
- [x] **[M3 P1] T-NET-1: TLS Trust Store / Insecure Toggle**; Goal: trust system CA bundle and allow a debug-only insecure flag; Scope: CurlNetwork TLS config + flag wiring; Acceptance: HTTPS succeeds with valid certs, optional bypass works; Tests: `ctest --preset user-ninja-multi-vcpkglt`.
- [x] **[M3 P1] T-NET-2: Compressed Response Handling**; Goal: decode gzip/br/zstd responses; Scope: CurlNetwork + vcpkg features; Acceptance: Content-Encoding responses are decoded into plain HTML; Tests: `ctest --preset user-ninja-multi-vcpkglt`.
- [x] **[M3 P2] T-REFAC-4: Consolidate Utilities + Include Paths**; Goal: reduce duplicate helpers and normalize includes; Scope: core/utils + include paths; Acceptance: helpers centralized, no include regressions; Tests: `ctest --preset user-ninja-multi-vcpkglt`.
