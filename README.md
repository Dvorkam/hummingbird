<p align="center">
  <img src="assets/icons/hummingbird-128.png" width="96" height="96" alt="Hummingbird logo">
</p>

<h1 align="center">Hummingbird Browser Engine</h1>

<p align="center">
  <strong>An experimental C++20 browser engine that turns HTML, CSS, and JavaScript into pixels.</strong>
</p>

<p align="center">
  <a href="https://github.com/Dvorkam/hummingbird/actions/workflows/ci.yml"><img src="https://github.com/Dvorkam/hummingbird/actions/workflows/ci.yml/badge.svg" alt="CI status"></a>
  <a href="https://github.com/Dvorkam/hummingbird/releases"><img src="https://img.shields.io/github/v/release/Dvorkam/hummingbird?include_prereleases" alt="Latest release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0--or--later%20%2B%20Section%207-blue" alt="AGPL-3.0 with Section 7 permission"></a>
</p>

Hummingbird implements its own HTML tokenizer and parser, DOM, CSS cascade, layout,
and paint pipeline instead of embedding Chromium or WebKit. QuickJS, libcurl, SDL2,
Blend2D, and image-decoding libraries provide the platform-level pieces.

This is a learning-driven engine with increasingly realistic proof targets. It is
interesting to experiment with, but it is **not yet a usable
general-purpose browser**.

> [!CAUTION]
> Hummingbird is not safe for untrusted browsing. It has no sandbox, site isolation,
> or permissions model. Most modern sites will render incorrectly, lose functionality,
> or fail to load. Use the built-in and documented proof pages when evaluating it.

## Quick look

<p align="center">
  <img src="https://github.com/user-attachments/assets/dc544503-d401-473d-a85b-6364680146e2" width="800" alt="Tour of Hummingbird browsing built-in demos and selected real pages">
</p>

<p align="center">
  <sub>A tour of built-in demos, tabs, DuckDuckGo, bookmarks, Hacker News, and a live article—all rendered by Hummingbird.</sub>
</p>

### Selected pages

<table>
  <tr>
    <td width="50%" valign="top">
      <a href="https://github.com/user-attachments/assets/4aa3832f-c868-4c74-85b6-8d0e2b95cd15">
        <img src="https://github.com/user-attachments/assets/4aa3832f-c868-4c74-85b6-8d0e2b95cd15" alt="DuckDuckGo HTML homepage rendered by Hummingbird">
      </a>
      <br>
      <strong>DuckDuckGo HTML</strong><br>
      <sub>The targeted homepage and search flow, not full DuckDuckGo compatibility.</sub>
    </td>
    <td width="50%" valign="top">
      <a href="https://github.com/user-attachments/assets/41fc6249-50c9-4921-8c2e-992175188d58">
        <img src="https://github.com/user-attachments/assets/41fc6249-50c9-4921-8c2e-992175188d58" alt="Hummingbird's built-in bookmarks page">
      </a>
      <br>
      <strong>Bookmarks and browser chrome</strong><br>
      <sub>An engine-owned <code>about:bookmarks</code> page with persisted entries.</sub>
    </td>
  </tr>
</table>

## What works today

The project advances through narrow proof targets. Passing one target means that flow
works; it does not imply broad compatibility with similar sites.

| Proof target | What it demonstrates | Important boundary |
| --- | --- | --- |
| Built-in `example.dev` pages | Deterministic HTML/CSS/layout/JS demonstrations | Designed around the implemented feature set |
| DuckDuckGo HTML | Real-page HTML/CSS, forms, navigation, and layout | Only the targeted flow and interactions are expected to work |
| Pinned vanilla TodoMVC | DOM mutation, events, timers, and incremental invalidation | A fixed local fixture, not arbitrary framework compatibility |
| Hacker News | Login, comment submission, persistent cookies, and restart-safe sessions | **Requires per-site compatibility mode; see below** |

<p align="center">
  <a href="https://github.com/user-attachments/assets/688f3e14-b0dc-45f6-a766-80af1369cdaa">
    <img src="https://github.com/user-attachments/assets/688f3e14-b0dc-45f6-a766-80af1369cdaa" width="800" alt="Hacker News rendered by Hummingbird with a compatibility-mode warning">
  </a>
</p>

<p align="center">
  <sub>Hacker News rendering and session proof. Login and comment submission require the explicit per-origin compatibility mode shown in the capture.</sub>
</p>

### Capability overview

**Document and rendering pipeline**

- HTML tokenization and parsing into an arena-backed DOM.
- CSS parsing, selector matching, cascade, `<style>` blocks, and external stylesheets.
- Block, inline, flexbox, grid, table, list, and positioned layout subsets.
- Web fonts, raster images, SVG, backgrounds, borders, shadows, and basic transforms.
- Blend2D painting into an SDL2 window.

**Scripting and interaction**

- QuickJS behind an engine-owned scripting interface.
- DOM queries and mutations, attributes, `classList`, and `dataset` subsets.
- Capture/target/bubble events, timers, microtasks, and `requestAnimationFrame`.
- Basic forms, text input, multiline `<textarea>`, focus, and GET/POST submission.

**Browser behavior**

- Multiple isolated tabs, back/forward navigation, reload, and bookmarks.
- HTML, CSS, image, SVG, and font resource loading through libcurl.
- Redirect handling, referrer/origin headers, and retryable network-error pages.
- An engine-owned RFC 6265-shaped cookie jar with persistence and
  `document.cookie`.
- Persistent per-origin `localStorage` and per-tab `sessionStorage`.
- A small experimental extension host with tab events and CSS injection.

### Major gaps

- HTML, CSS, DOM, and JavaScript APIs cover only a small subset of the web platform.
- Most production websites are visually broken or functionally unusable.
- There is no `fetch`/XHR, full framework compatibility, media playback, canvas,
  WebGL, service workers, or browser-grade accessibility.
- Text editing, selection, shaping, bidirectional text, and form controls are partial.
- There is no security boundary suitable for browsing hostile content.

The long-term roadmap ends with YouTube playback and a scrolling TikTok feed. Those
are **aspirational endgame targets**, not current capabilities. See the
[roadmap](doc/milestones/roadmap.md) for the compatibility ladder and its non-goals.

## Try a prerelease

Prebuilt artifacts are published on [GitHub Releases](https://github.com/Dvorkam/hummingbird/releases):

| Platform | Artifact | Validation status |
| --- | --- | --- |
| Windows x64 | `Hummingbird-<version>-win64.zip` | Primary development platform; exercised manually and in CI |
| Linux x86-64 | `Hummingbird-<version>-linux-x86_64.AppImage` | Built and smoke-tested in Ubuntu CI; additional browser tests run under Fedora in WSL, but native Linux desktop use is not regularly validated |

### Windows

Extract the zip and run `hummingbird.exe`. Keep the `assets/` directory next to the
executable.

### Linux AppImage

```bash
chmod +x ./Hummingbird-*-linux-x86_64.AppImage
./Hummingbird-*-linux-x86_64.AppImage
```

If FUSE support is unavailable, extract and run the AppImage:

```bash
./Hummingbird-*-linux-x86_64.AppImage --appimage-extract
cd squashfs-root
HB_ASSET_ROOT="$PWD/usr/share/hummingbird" ./usr/bin/hummingbird
```

Reports from native Linux desktops are especially welcome.

## Controls

Hummingbird has deliberately minimal browser chrome, so many actions use keyboard
shortcuts.

| Shortcut | Action |
| --- | --- |
| `Ctrl+L`, then `Enter` | Focus the URL bar and navigate |
| `Alt+Left` / `Alt+Right` | Back / forward |
| `F5` / `Ctrl+R` | Reload |
| `Ctrl+T` / `Ctrl+W` | Open / close a tab |
| `Ctrl+Left` / `Ctrl+Right` | Switch tabs |
| `Ctrl+D` | Bookmark the current page |
| `Ctrl+Shift+O` | Open `about:bookmarks` |
| `Ctrl+Shift+U` | Toggle compatibility mode for the current origin |
| `F1` | Toggle debug outlines |
| Mouse wheel | Scroll |

Startup opens `https://example.dev`, the built-in demonstration hub. It links to
milestone pages and focused fixtures for TodoMVC, JavaScript, cookies, storage,
textarea input, and network errors.

## Browser identity and compatibility mode

Hummingbird identifies itself honestly by default: its `User-Agent` and
`Sec-CH-UA` client hint identify Hummingbird rather than Chromium.

Some servers reject unknown browser identities before the engine gets a chance to
render their response. Compatibility mode is an explicit, per-origin escape hatch:

- Press `Ctrl+Shift+U` to give the current origin a canonical Chrome-shaped
  `User-Agent`.
- `Sec-CH-UA` continues to identify Hummingbird.
- The choice persists across restarts and is never enabled automatically.
- Toggling the mode never silently replays a form POST.

Press `Ctrl+Shift+U` again to return that origin to Hummingbird's normal identity.

## Architecture

The core page path is:

```text
network response
      ↓
HTML tokenizer/parser → DOM → style/cascade → layout tree → display list → paint
                              ↕
                        QuickJS bindings
```

Hummingbird follows Ports & Adapters. Platform-independent modules do not depend on
SDL, Blend2D, libcurl, or QuickJS implementations.

- `src/app`: browser chrome, event loop, and application wiring.
- `src/core`: shared types, DOM, storage/network policy, and platform interfaces.
- `src/html`: HTML tokenizer and parser.
- `src/style`: CSS parsing, selector matching, cascade, and computed style.
- `src/layout`: render tree and block/inline/flex/grid/table layout.
- `src/renderer`: display-list construction and painting.
- `src/engine`: documents, tabs, resources, scripting, navigation, and extensions.
- `src/platform`: SDL, Blend2D, libcurl, QuickJS, and decoder adapters.

The detailed rules live in the [coding constitution](doc/coding_constitution.md),
and the [interactive page-pipeline diagram](doc/diagrams/page_pipeline.html) provides
a deeper tour.

## Building from source

### Prerequisites

- A C++20 compiler (MSVC, Clang, or GCC)
- CMake 3.20 or newer
- Ninja
- vcpkg with `VCPKG_ROOT` set

Dependencies are declared in `vcpkg.json` and installed during CMake configure.

### Windows

The PowerShell scripts enter the MSVC development environment automatically:

```powershell
.\scripts\build.ps1
.\scripts\test.ps1
.\build\Release\Hummingbird.exe
```

### Linux

Ubuntu/Debian packages roughly matching CI:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential ninja-build \
  autoconf autoconf-archive automake libtool libltdl-dev \
  libx11-dev libxext-dev libxft-dev xvfb patchelf file curl
```

Configure, build, test, and run:

```bash
export VCPKG_ROOT="$HOME/path/to/vcpkg"
cmake --preset ninja-multi-vcpkg
cmake --build --preset ninja-multi-vcpkg --config Release
ctest --preset ninja-multi-vcpkg -C Release --output-on-failure
./build/Release/Hummingbird
```

Set `HB_RUN_SMOKE_TEST=1` when running `ctest` to include the window-opening smoke
test. Add `HB_HEADLESS=1` for a headless CI-style run.

## Quality gates

- Windows and Ubuntu builds, unit tests, and smoke tests run in GitHub Actions.
- Pinned DuckDuckGo, TodoMVC, and login/session fixtures guard proof flows.
- Curated web-platform-test slices cover selected compatibility areas.
- HTML and CSS parser fuzzers run under libFuzzer and AddressSanitizer in CI.
- Dependency-firewall tests enforce module direction and reject package cycles.

## Extensions

The extension host is a deliberately small experiment, not a WebExtensions-compatible
implementation. It supports a manifest, long-lived background scripts, selected
`browser.tabs` events, and CSS injection. A bundled dark-mode extension is the first
consumer. See [Extension host MVP](doc/extensions.md) for its API and configuration.

## Troubleshooting

<details>
<summary>TLS certificate problems (debugging only)</summary>

If HTTPS fails because libcurl cannot find a suitable CA bundle, point it at system
roots with `CURL_CA_BUNDLE`, `SSL_CERT_FILE`, or `SSL_CERT_DIR`.

For debugging only, certificate verification can be disabled:

```bash
HB_TLS_INSECURE=1 ./build/Release/Hummingbird
```

Do not use this mode for ordinary browsing.

</details>

## Documentation

- [Roadmap and milestone status](doc/milestones/roadmap.md)
- [Live backlog](doc/TODOs.md)
- [Architecture and coding rules](doc/coding_constitution.md)
- [Developer workflow guides](doc/dev_guide/)
- [Contributing guide](CONTRIBUTING.md)
- [Changelog](CHANGELOG.md)

## License

Hummingbird is licensed under AGPL-3.0-or-later, with an additional section 7
permission allowing app-store distribution. See [LICENSE](LICENSE), [NOTICE](NOTICE),
and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
