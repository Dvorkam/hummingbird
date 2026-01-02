# Contributing

Thanks for your interest in Hummingbird.

This is an early, fast-moving prototype and I may not be able to review or merge external PRs quickly. If you want to help, the best first step is to open an issue describing what you want to work on, or reach out to the author directly.

## What I’m looking for

- Bug reports with repro steps (and screenshots/logs when possible)
- Small, focused fixes with tests
- Documentation improvements

For larger changes (new subsystems, big refactors, new dependencies), please ask first.

## Development workflow

- Formatting: keep code `clang-format`’d (CI checks formatting under `src/`).
- Build/test locally:
  - Configure/build: `cmake --preset ninja-multi-vcpkg && cmake --build --preset ninja-multi-vcpkg --config Release`
  - Tests: `ctest --preset ninja-multi-vcpkg -C Release --output-on-failure`
  - Optional smoke test (opens a window): `HB_RUN_SMOKE_TEST=1 ctest --preset ninja-multi-vcpkg -C Release --output-on-failure`

## Licensing

By contributing, you agree that your contributions are licensed under the project’s license (Apache-2.0, see `LICENSE`).

If you add or bundle third-party code/assets, include the relevant license text in the repository (and keep it close to the asset when possible, e.g. `assets/.../LICENSE.txt`).
