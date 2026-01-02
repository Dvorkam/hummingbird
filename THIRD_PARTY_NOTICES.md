# Third-party notices

This document lists third-party components used by Hummingbird and where their license texts are stored.

## Bundled in this repository

- Roboto fonts (`assets/fonts/Roboto-*.ttf`) — Apache License 2.0  
  License text: `assets/fonts/Roboto-LICENSE.txt`

## Dependencies (via vcpkg)

Hummingbird uses vcpkg to fetch/build third-party libraries declared in `vcpkg.json`.

- SDL2
- Blend2D
- libcurl
- GoogleTest (tests only)

When building from source with vcpkg, vcpkg installs per-port license texts at:

`vcpkg_installed/**/share/<port>/copyright`

Release artifacts bundle these (best-effort) under:

- Windows zip: `licenses/vcpkg/` (or `licenses/vcpkg-<port>.txt`)
- Linux AppImage: `usr/share/hummingbird/licenses/vcpkg/*.txt`

## Build-time tools (not bundled)

CI/release packaging uses tools such as CMake, Ninja, and (for Linux AppImage packaging) linuxdeploy. These are not shipped inside the repository as redistributable components.
