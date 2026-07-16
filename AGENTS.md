# Agent Notes

Hummingbird is a C++20 browser engine (HTML → DOM → style → layout → paint) built as
strictly separated modules. Read the pointers below instead of re-deriving project
rules; keep this file thin — detailed rules live in the linked docs.

## Document map

- `doc/coding_constitution.md` — architecture and coding rules. **All generated code
  must adhere to it** (Ports & Adapters firewall, arena memory, no exceptions, naming).
- `doc/milestones/roadmap.md` — milestone index and statuses. The active milestone's
  `doc/milestones/milestoneN.md` defines the current scope and story format.
- `doc/TODOs.md` — live backlog index.
- `doc/dev_guide/` — step-by-step guides for recurring workflows (e.g. adding CSS
  properties). Check for a matching guide before implementing; if you repeat a
  workflow that has no guide, add one.
- `doc/tech_debt_game_plan.md` — tracked refactoring seams. When touching a file
  listed there, prefer the planned decomposition over ad-hoc changes.

## Workflow rules

- If the current story conflicts with `doc/milestones/roadmap.md`, point it out and
  suggest the architecturally cleanest resolution before coding.
- If a new task appears during work that does not fit the current scope, add it to
  `doc/TODOs.md` as a future story (match the existing `T-*` story format).
- **Demo every user-visible feature.** When a change adds or extends something
  observable on a page (a CSS property/value, a layout mode, form/JS behavior),
  also represent it on the demo site under `assets/stub/pages/` (served as
  `example.dev/<name>`): add a new element or extend an existing one on the
  relevant milestone's page, and update that page's "Added in MN" note. These
  pages are living documentation *and* the manual test surface, so a user-visible
  feature that ships without a demo entry is incomplete. Purely internal changes
  (perf, security, refactors, tooling, parser plumbing with no new rendered
  result) are exempt.
- **Never work around a bug.** Code that masks or dodges a defect is not an
  acceptable resolution. When you hit a bug:
  - If it is small and blocks the current story: fix it properly, then note the
    fix (commit message / relevant doc) and continue.
  - If fixing it is large or risky: stop and defer the current work until the bug
    is fixed rather than building on top of it.
  - If it has no bearing on the current task: note it (add to `doc/TODOs.md`) and
    keep working on the task — do not derail, and do not paper over it.
- **Note code smells you notice.** The codebase should get healthier over time,
  not worse. When work surfaces a design smell — shotgun surgery (one feature
  forces the same mechanical edit across many files/layers), duplicated logic,
  a leaky abstraction, a boilerplate-heavy extension point — file it in
  `doc/TODOs.md` as a `refactor`/tech-debt `T-*` story, even if you don't act on
  it now. Don't refactor mid-feature (keep the diff focused and mirror the
  established pattern), but don't let the observation evaporate either. Prefer
  paying the debt down at the next natural touch point.
- Make a git commit after every self-contained change, in conventional format:
  `feat|fix|chore|docs|refactor|test: message`.

## Before every commit that changes code

1. Build: `scripts/build.ps1` on Windows (it enters the MSVC dev shell
   automatically and picks the local preset when `CMakeUserPresets.json`
   exists). On Linux: `cmake --preset ninja-multi-vcpkg && cmake --build
   --preset ninja-multi-vcpkg --config Release`.
2. Test: `scripts/test.ps1` on Windows (add `-Filter <regex>` for a subset);
   `ctest --preset ninja-multi-vcpkg -C Release --output-on-failure` on Linux.
3. Format: `clang-format -i` over the touched files under `src/` (CI enforces
   formatting for `src/**`).

Note for Windows: raw `cmake --preset` requires an MSVC dev environment
(`cl` on PATH); the scripts exist precisely so you never need to set that up
manually.

## Include audits (when asked, or when includes look bloated)

- Use include-what-you-use if available: `iwyu_tool.py -p build`, apply suggestions
  with `fix_includes.py`.
- Requires `CMAKE_EXPORT_COMPILE_COMMANDS=ON` so IWYU can read
  `build/compile_commands.json`.
