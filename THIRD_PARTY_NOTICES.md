# Third-party notices

This document lists third-party components used by Hummingbird and where their license texts are stored.

## Bundled in this repository

- Roboto fonts (`assets/fonts/Roboto-*.ttf`) — Apache License 2.0  
  License text: `assets/fonts/Roboto-LICENSE.txt`
- Roboto Mono fonts (`assets/fonts/RobotoMono-*.ttf`) — SIL Open Font License 1.1  
  License text: `assets/fonts/Roboto_Mono/OFL.txt`
- Public Suffix List (`src/core/net/PublicSuffixData.h`, generated) — Mozilla Public
  License 2.0  
  Source: <https://github.com/publicsuffix/list> (`public_suffix_list.dat`), pinned
  to the upstream commit recorded in the generated header and refreshed by
  `scripts/update_public_suffix_list.ps1`.  
  License text: <https://mozilla.org/MPL/2.0/>
- Public Suffix List conformance vectors (`tests/fixtures/public_suffix_tests.txt`,
  tests only) — dedicated to the public domain under CC0 1.0, per the file's own
  header. Vendored from the same upstream commit as the rule data.

## Dependencies (via vcpkg)

Hummingbird uses vcpkg to fetch/build third-party libraries declared in `vcpkg.json`.

- SDL2
- Blend2D
- libcurl
- GoogleTest (tests only)

When building from source with vcpkg, vcpkg installs per-port license texts at:

`**/vcpkg_installed/**/share/<port>/copyright` (often under `build/vcpkg_installed/...` when using the provided CMake preset)

Release artifacts bundle these (best-effort) under:

- Windows zip: `licenses/vcpkg/*.txt`
- Linux AppImage: `usr/share/hummingbird/licenses/vcpkg/*.txt`

## Build-time tools (not bundled)

CI/release packaging uses tools such as CMake, Ninja, and (for Linux AppImage packaging) linuxdeploy. These are not shipped inside the repository as redistributable components.
