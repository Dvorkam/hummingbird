# Changelog

All notable changes to this project will be documented in this file.

## [0.5.0] - 2026-02-10

### Added

- Multi-tab browser workflow with create/switch/close controls and per-tab isolation.
- Extension host MVP with manifest loading, background script runtime, tab lifecycle events, and CSS injection API.
- Built-in dark-mode extension behavior that applies across ordinary loaded pages.
- Form interaction and submission improvements including autofocus behavior, robust focus/edit handling, and GET/POST submission paths.
- Table layout/readability improvements with additional regression coverage and external-page verification checklist.
- `BrowserApp` decomposition via `BrowserChrome` and `TabController` to reduce app-level orchestration bloat.

### Improved

- CSS/layout compatibility slice for real pages (including typography and control polish areas used by DDG-like pages).
- Font-family fallback handling for collapsed/quoted family lists to reduce false fallback warnings.
- Documentation for Milestone 5 outcomes and carryover stories.

### Deferred

- Remaining DDG visual parity gaps and snapshot-based DDG end-to-end regression harness are tracked in Milestone 6 backlog (`doc/TODOs.md`).
