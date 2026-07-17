# Architecture Diagrams (clang-uml)

Generated architecture diagrams live in [`doc/diagrams/`](../diagrams/) as
PlantUML (`.puml`). They are produced by [clang-uml](https://github.com/bkryza/clang-uml)
from the compilation database and the config at the repo root: `clang-uml.yaml`.

## What we diagram (and what we don't)

The diagrams cover only the **portable layers** — `app`, `core`, `engine`,
`html`, `layout`, `renderer`, `style` — and the **platform port**
(`src/core/platform_api`). The platform **adapters** (`src/platform/*` — the
SDL / Blend2D / curl / lunasvg glue) are intentionally **not** parsed:

- They drag in third-party headers that libclang cannot always parse on the
  MSVC toolchain (e.g. SDL's `_m_prefetch` intrinsic), and
- UML-ing third-party glue is noise, not signal.

The architectural invariant that actually matters — that nothing depends
*inward* on the adapters (Ports & Adapters) — is enforced by the
`DependencyFirewall` test (T-ARCH-GUARD-1), not by these pictures.

Each diagram's `glob` selects our source minus the adapters via the regex
`.*src.(app|core|engine|html|layout|renderer|style).*\.cpp$` (the `.` after
`src` matches either path separator).

## The diagrams

| Diagram | Type | Scope |
|---|---|---|
| `hummingbird_packages` | package | Portable layers as directory packages + their dependency edges (shows the platform_api port as a leaf). |
| `platform_api` | class | The platform **port**: interfaces (`IGraphicsContext`, `INetwork`, `IResourceProvider`, `IImageDecoder`, `IScriptEngine`, `IWindow`, …) + the value types crossing it. |
| `engine_document_pipeline` | class | The engine's own classes (`Hummingbird::Engine` namespace only) across `document`/`resources`/`script`/`tab`/`forms`/`extensions`. |

Noise control: class diagrams filter to our namespaces (`Hummingbird` /
`Hummingbird::Engine`) and exclude anonymous members (`.*anonymous.*`), which
libclang otherwise emits once per translation unit (e.g. `InputEvent`'s union
payloads — ~110 phantom classes before filtering).

## Regenerating

Requires `clang-uml` on `PATH` and a fresh compilation database at
`build/compile_commands.json` (CMake writes it when configured with
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`, which our presets do).

```sh
# All diagrams (packages is the slow one, ~6 min; use all cores with -t 0):
clang-uml --config clang-uml.yaml -t 0 --progress

# A single diagram:
clang-uml --config clang-uml.yaml -n platform_api -t 0
```

Render a `.puml` to SVG/PNG with any PlantUML tool if a picture is needed; the
`.puml` sources are what we commit and review.

## Notes / gotchas

- clang-uml re-parses every globbed translation unit **per diagram**, so keep
  each diagram's `glob` as narrow as its scope allows (the class diagrams only
  glob the TUs they need, not the whole tree).
- clang parses the MSVC-flavoured compile commands; deprecation `[WARNING]`s
  from system headers are expected and harmless. A real `-Wswitch` warning in
  *our* code, though, is worth fixing — regenerating these diagrams surfaced two
  unhandled `Display::Grid` switches during T-ARCH-GUARD-2.
