# Pre-PR Checklist

Run before opening a PR or asking for review. Every item here exists because
something got through without it — the incident is named so the item can be
deleted honestly if it ever stops being true.

Keep it that way: **do not add a step without a reason someone can check.**
A checklist of plausible-sounding advice is one nobody reads twice.

---

## 1. Build, test, format

The three steps from `AGENTS.md`. Non-negotiable, and they are the cheap part.

- [ ] `scripts/build.ps1` succeeds (Windows) — add `-LogLevel INFO` if you will
      then be reading logs from a run
- [ ] `scripts/test.ps1` is fully green, and the **count went up** if you added
      tests. A count that did not move means your new file is not in
      `tests/CMakeLists.txt`
- [ ] `clang-format -i` over every touched file under `src/` (CI enforces this
      for `src/**` only, but running it on `tests/` too keeps diffs quiet)

## 2. Portability — the local compiler is the permissive one

**This is the item that has cost the most CI rounds.** A green MSVC build says
much less than it looks like it does, and there is no Linux toolchain in the
sandbox, so CI is the only thing that can catch this class. Check it by reading,
before pushing.

- [ ] **Every `std::` name you used has its own `#include`.** Do not rely on one
      arriving through another header — MSVC's headers include far more than
      libstdc++ and libc++ do.
      *Incident: `std::unique_lock` used with only `<shared_mutex>` included.
      `<shared_mutex>` provides the mutex and `std::shared_lock`; `unique_lock`
      lives in `<mutex>`. Failed both the Ubuntu build and the fuzzer.*
      Common traps: `std::cout`/`std::endl` → `<iostream>`, `std::pair` →
      `<utility>`, `std::move` → `<utility>`, `std::size_t` → `<cstddef>`,
      `std::to_string` → `<string>`, `std::function` → `<functional>`.
- [ ] **No raw control bytes inside character literals.** Write `'\r'`, never a
      literal CR byte between quotes.
      *Incident: `tests/core/Log.test.cpp` held bytes `27 0d 27`. GCC reads the
      bare CR as a line terminator and reports "missing terminating ' character";
      MSVC accepts it silently.*
- [ ] **No default argument in a member function that names a nested aggregate's
      default member initializer.** GCC enforces the standard here, MSVC does
      not. See the comment above `ResourceLoader::send_request`.
- [ ] **When CI does go red, audit the whole changed set — not just the file it
      named.** A build stops at the first error, so one red round routinely
      hides several independent problems.
      *Incident: fixing the missing `<mutex>` let the build reach a translation
      unit it had never compiled, which then failed on the raw CR. Auditing all
      files added in that milestone caught three more missing includes before
      they cost further rounds.*

## 3. Are the guards actually load-bearing?

A test written after the code can pass for the wrong reason. The cheap check is
to break the thing on purpose.

- [ ] For each **security- or correctness-critical** guard added: disable it,
      rebuild, and confirm **exactly the expected test fails** — no more, no
      fewer. Restore it and note the result in the commit message.
      *Incidents: the cookie dot-boundary check, userinfo stripping in host
      extraction, the per-hop filter gate, and the extension permission gate
      were all confirmed this way. The permission mutation also exposed a test
      that had been passing for the wrong reason — it asserted `false` on a path
      that returned `false` for an unrelated missing handler.*
- [ ] Any test asserting "nothing happened" **must** be shown to fail when the
      thing is allowed to happen. Otherwise it may be passing vacuously.

## 4. Tests assert on the observable, not on self-reports

- [ ] Assertions are on what a **neighbouring layer saw** — painted text, which
      transport ran, what the fake server was asked for — rather than on a
      component's report about itself.
      *Incident: a filter that counts a block while the transport still sends
      the request would pass a self-report assertion and fail a "what did the
      server see" one.*
- [ ] Fixtures are built the way the **pipeline** builds them, not by hand.
      Hand-built fixtures have passed for the wrong reason and hidden shipped
      bugs.
- [ ] Anything time-dependent uses an **injected clock**, not the wall clock.

## 5. Logs a human will read

- [ ] Nothing logs at `WARN`/`ERROR` during **correct** operation. Warnings that
      fire normally are how real warnings stop being read.
      *Incidents: blocked requests logged as "fetch failed" and "external script
      skipped"; requests abandoned at shutdown logged as curl failures. Both
      shipped, both were user-reported.*
- [ ] Log messages state what was **observed**, not a guessed cause.
      *Incident: "almost certainly out of memory" was inferred from an SDL
      failure and was wrong — the real error was a malformed bitmap from a
      use-after-free. The wrong message misdirected the next investigation.*

## 6. Docs are in sync

- [ ] `doc/TODOs.md` — finished stories ticked; new work filed with a `T-*` id;
      anything re-tagged to another milestone carries the reason inline
- [ ] `doc/milestones/milestoneN.md` — checklist ticked. **Cross-check against
      `TODOs.md`**: the two registers drift, and a stale checkbox on a P0 is
      what makes a milestone look unfinished (or finished) when it is not
- [ ] `doc/conformance/*.md` — if you changed a module with a register, update
      its status line and the number its ratchet prints
- [ ] A **demo** exists for anything user-visible, under `assets/stub/pages/`,
      with the "Added in MN" note updated (`AGENTS.md` workflow rule)
- [ ] Code smells noticed along the way are filed, not just noticed

## 7. What you could not verify

- [ ] State it explicitly, in the PR body and the commit message. The sandbox
      cannot launch `Hummingbird.exe`, so anything needing the running browser
      is a **manual gate** and must be named as one rather than implied to be
      covered.
- [ ] Name the live check the reviewer should perform, concretely enough to
      follow: which page, what to look for, what the log should say.

---

## After pushing

- [ ] `gh pr checks <N>` is green on **the same SHA as HEAD** — confirm the run's
      `head_sha`, since a green run against an older commit proves nothing
- [ ] If a check fails, go back to §2 before assuming the failure is local to the
      file named
