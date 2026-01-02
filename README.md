# Hummingbird Browser Engine

Hummingbird is an experimental browser engine built from scratch in C++20 (HTML → DOM → layout → paint). It’s primarily an educational project.

## Status / expectations

This is an early prototype:

- It is **not a secure browser** (no sandboxing, no site isolation, no permissions model).
- There is **no JavaScript**.
- HTML/CSS support is partial and changes frequently.

## What works today (high level)

- HTML tokenizer + parser building a DOM tree.
- CSS parsing for a subset of selectors/properties, including `<style>` blocks.
- Basic block + inline layout (with ongoing work on inline flow).
- Painting via Blend2D into an SDL2 window.
- Fetching HTML via libcurl, plus a deterministic built-in demo page at `https://example.dev`.

## Getting started (prebuilt releases)

Releases are published on GitHub as:

- **Linux AppImage**: `Hummingbird-<version>-linux-x86_64.AppImage`
- **Windows portable zip**: `Hummingbird-<version>-win64.zip`

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
  libx11-dev libxft-dev libxext-dev \
  autoconf autoconf-archive automake libtool libltdl-dev
```

If you want to run tests headlessly, also install `xvfb` (CI uses it).

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

## Usage / controls

- `Ctrl+L`: focus URL bar
- `Enter`: navigate
- `Esc`: unfocus URL bar
- Mouse wheel: scroll
- `F1`: toggle debug outlines

Startup defaults to `https://example.dev` (a built-in demo page). Loading arbitrary sites is best-effort and incomplete.

## License

Hummingbird is licensed under the Apache License 2.0 (see `LICENSE` and `NOTICE`).

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
