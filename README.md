# Hummingbird Browser Engine

Hummingbird is an experimental browser engine built from scratch in C++20 (HTML → DOM → layout → paint). It’s primarily an educational project.

## Quick look

Stub site + navigation demo:

![Stub site and navigation demo](https://github.com/user-attachments/assets/98e01801-d678-40fd-ab7c-190b68502075)

## Status / expectations

This is an early prototype:

- It is **not a secure browser** (no sandboxing, no site isolation, no permissions model).
- JavaScript support is **minimal** (QuickJS with a small binding/event surface).
- HTML/CSS support is partial and changes frequently.

## What works today (high level)

- Multi-tab app flow (create/switch/close) with per-tab isolation.
- Extension host MVP (manifest + loader + long-lived background scripts + `browser.tabs` events + CSS injection).
- HTML tokenizer + parser building a DOM tree.
- CSS parsing for a subset of selectors/properties, including `<style>` blocks and external stylesheets.
- Resource pipeline for HTML/CSS/images/SVG with incremental restyles as data arrives.
- URL normalization + relative URL resolution for linked resources.
- Block + inline layout, list markers, table layout, and key positioning/box-model features (including percent sizing/positioning and table width-hint balancing).
- Form controls (`<form>`, `<input>`, `<button>`), focus/editing, `autofocus`, GET+POST submit flows, and external submit controls.
- Real-page CSS polish coverage including border-radius/outline/box-shadow, text effects, overflow handling, cursor, and vertical-align.
- Background images and basic transforms used by real-world pages.
- QuickJS integration with `onclick`/`load` dispatch and basic DOM mutation bindings.
- Painting via Blend2D into an SDL2 window.
- Image decoding via SDL2_image + SVG decoding via lunasvg.
- Fetching HTML via libcurl, plus a deterministic built-in demo page at `https://example.dev`.

## Getting started (prebuilt releases)

Releases are published on GitHub as:

- **Linux AppImage**: `Hummingbird-<version>-linux-x86_64.AppImage`
- **Windows portable zip**: `Hummingbird-<version>-win64.zip`

Release highlights are tracked in `CHANGELOG.md`.

### Linux (AppImage)

1. Download the `.AppImage` from GitHub Releases.
2. Run it:

   ```bash
   chmod +x ./Hummingbird-*-linux-x86_64.AppImage
   ./Hummingbird-*-linux-x86_64.AppImage
   ```

If your distro doesn’t support running AppImages (often missing `fuse2`), you can extract and run:

```bash
./Hummingbird-*-linux-x86_64.AppImage --appimage-extract
cd squashfs-root
HB_ASSET_ROOT="$PWD/usr/share/hummingbird" ./usr/bin/hummingbird
```

### Windows (zip)

1. Download the `.zip` from GitHub Releases and extract it.
2. Run `hummingbird.exe` from the extracted folder.

Keep the `assets/` folder next to the executable (fonts, UA stylesheet, etc).

## Building from source

This project uses a `vcpkg.json` manifest; dependencies are installed by vcpkg during CMake configure.

### Prerequisites

- C++20 compiler (MSVC / Clang / GCC)
- CMake ≥ 3.20
- Ninja
- vcpkg with `VCPKG_ROOT` set

Linux packages (Ubuntu/Debian) roughly matching CI:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential ninja-build \
  autoconf autoconf-archive automake libtool libltdl-dev \
  xvfb patchelf file curl
```

Depending on your distro, SDL2’s X11/Wayland backends may need system dev packages
(for example: `libx11-dev`, `libxext-dev`, `libxft-dev` on Ubuntu/Debian).

### Build steps (using presets)

```bash
export VCPKG_ROOT="$HOME/path/to/vcpkg"
cmake --preset ninja-multi-vcpkg
cmake --build --preset ninja-multi-vcpkg --config Release
```

Run:

```bash
./build/Release/Hummingbird
```

### Tests

```bash
ctest --preset ninja-multi-vcpkg -C Release --output-on-failure
```

The smoke test that opens a window is guarded; enable it with:

```bash
HB_RUN_SMOKE_TEST=1 ctest --preset ninja-multi-vcpkg -C Release --output-on-failure
```

For headless CI-like runs (especially on Windows), use:

```bash
HB_RUN_SMOKE_TEST=1 HB_HEADLESS=1 ctest --preset ninja-multi-vcpkg -C Release --output-on-failure
```

## Test coverage

- CI: Windows + Ubuntu (unit tests + smoke test via GitHub Actions).
- Manual visual checks: Fedora (local run).

## TLS troubleshooting (debug only)

If HTTPS fails due to missing or unusual CA bundles, libcurl will fall back to stub
content. You can point it at system roots with:

- `CURL_CA_BUNDLE=/path/to/ca-bundle.crt`
- `SSL_CERT_FILE=/path/to/ca-bundle.crt`
- `SSL_CERT_DIR=/path/to/certs`

For debugging only, you can bypass verification entirely:

```bash
HB_TLS_INSECURE=1 ./build/Release/Hummingbird
```

## Usage / controls

- `Ctrl+L`: focus URL bar
- `Enter`: navigate
- `Esc`: unfocus URL bar
- Mouse wheel: scroll
- `F1`: toggle debug outlines
- `Ctrl+T`: new tab
- `Ctrl+W`: close active tab
- `Ctrl+Left` / `Ctrl+Right`: switch tabs

Startup defaults to `https://example.dev` (a built-in demo page). Loading arbitrary sites is best-effort and incomplete.
JS demo page: `https://example.dev/js`.

## Extensions (MVP)

Milestone 5 introduces a minimal extension host. This is not WebExtension-compatible; it is a small, evolving surface.

What works:

- Load extensions from a directory on startup (default: `assets/extensions/`).
- Parse a minimal `manifest.json`.
- Run a long-lived background script once at startup (one QuickJS context per extension).
- `console.log(...)` works inside background scripts.
- `browser.tabs` subset:
  - `browser.tabs.active()`
  - `browser.tabs.onCreated.addListener(fn)`
  - `browser.tabs.onActivated.addListener(fn)`
  - `browser.tabs.onNavigated.addListener(fn)` where `fn` receives `{ id, url, active }`
- `browser.scripting.insertCSS({ tabId, cssText })`.
- Built-in `dark-mode` extension that injects CSS when tabs are created/activated/navigated.
- Enable/disable extensions via environment variables.

What is not implemented yet:

- No content scripts and no direct per-tab DOM access from extensions.
- No `removeCSS` API.
- No permission enforcement, sandboxing, or security model.
- No persistence for extension state across restarts.

### Built-in dark mode demo

- Open `https://example.dev/m5` and find the `Extension dark mode` section (the landing page at `https://example.dev` links every per-milestone demo page).
- The built-in dark-mode extension now applies across ordinary page content (with targeted readability safeguards), not only a demo-only scope class.
- Disable it with `HB_EXTENSIONS_DISABLE=dark-mode`.

### Directory layout

Extensions live in subdirectories under the extensions root. The extension ID is the directory name.

Example:

```
assets/extensions/dark-mode/
  manifest.json
  background.js
```

### Manifest format (v0)

Required fields:

- `name`: string
- `version`: string
- `background.entry`: string (relative path to the background script)

Optional fields:

- `permissions`: string[]

Example `manifest.json`:

```json
{
  "name": "Dark Mode",
  "version": "0.1.0",
  "background": { "entry": "background.js" },
  "permissions": ["tabs", "scripting"]
}
```

### Configuration

Startup now reads `assets/config/browser.ini` (or `HB_SETTINGS_INI` when set), so extension states can be changed without recompiling.

Example:

```ini
[extensions]
dark-mode = disabled
```

Accepted values: `enabled|disabled`, `true|false`, `on|off`, `yes|no`, `1|0`.

Environment variables still work and take precedence over INI:

- `HB_EXTENSIONS_DIR`: overrides the extensions root directory (defaults to `assets/extensions`).
- `HB_EXTENSIONS_DISABLE`: comma-separated list of extension IDs to disable.
- `HB_EXTENSIONS_ENABLE`: comma-separated allow-list of extension IDs to enable (when set, only these load).
- `HB_SETTINGS_INI`: overrides the settings file path (defaults to `assets/config/browser.ini`).

Examples:

```bash
# Disable the built-in dark-mode extension (directory name is the ID).
HB_EXTENSIONS_DISABLE=dark-mode ./build/Release/Hummingbird

# Enable only two extensions.
HB_EXTENSIONS_ENABLE=dark-mode,my-ext ./build/Release/Hummingbird
```

## Documentation

- Roadmap and milestone status: [`doc/milestones/roadmap.md`](doc/milestones/roadmap.md)
- Live backlog: [`doc/TODOs.md`](doc/TODOs.md)
- Architecture and coding rules: [`doc/coding_constitution.md`](doc/coding_constitution.md)
- Developer workflow guides: [`doc/dev_guide/`](doc/dev_guide/)
- Agent/automation entry point: [`AGENTS.md`](AGENTS.md)

## License

Hummingbird is licensed under the GNU Affero General Public License v3.0 or later
(AGPL-3.0-or-later), with an additional permission under section 7 allowing app-store
distribution (see `LICENSE`). Third-party attribution notices are listed in `NOTICE`.

This repository also contains third-party components. See `THIRD_PARTY_NOTICES.md`.

## Contributing

See `CONTRIBUTING.md`.

## Architecture

The project follows Ports & Adapters. Core logic stays decoupled from platform implementations.

-   `src/app`: application wiring (event loop, pipeline orchestration).
-   `src/core`: foundational types (arena allocator, asset paths) and interfaces (`IWindow`, `IGraphicsContext`, `INetwork`).
-   `src/html`: HTML tokenizer and DOM builder.
-   `src/style`: CSS tokenizer/parser, selector matching, and computed style production.
-   `src/layout`: render objects, tree builder, block + inline layout.
-   `src/renderer`: painter that walks the render tree.
-   `src/platform`: SDL window/graphics and Curl networking adapters.
