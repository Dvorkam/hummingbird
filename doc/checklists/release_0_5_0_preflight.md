# Release 0.5.0 Preflight

Date: 2026-02-10

## Completed

- [x] Milestone 5 tracking closed for release scope; completed stories archived (`doc/todo_archive/milestone5_done.md`).
- [x] Deferred DDG follow-up items moved into Milestone 6 backlog (`doc/TODOs.md`).
- [x] README updated with Milestone 5 feature set and dark-mode behavior.
- [x] Project version set to `0.5.0` in `CMakeLists.txt`.
- [x] Release highlights written to `CHANGELOG.md`.
- [x] Build passed: `cmake --build --preset user-ninja-multi-vcpkglt`.
- [x] Tests passed: `ctest --preset user-ninja-multi-vcpkglt --output-on-failure` (353/353, smoke test skipped by default).

## Pending Manual Steps Before Release

- [ ] Run final manual DDG sanity pass (focus, type, submit, navigate results) and record screenshots.
- [ ] Confirm no debug-only logging/noise is enabled in release defaults.
- [ ] Merge branch into `master`.
- [ ] Create and push release tag `v0.5.0`.
- [ ] Verify GitHub Actions `release.yml` artifacts (Linux AppImage + Windows zip).
- [ ] Publish GitHub release notes from `CHANGELOG.md`.
