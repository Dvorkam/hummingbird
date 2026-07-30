# Extension Host MVP

Hummingbird includes a small experimental extension host. It is not compatible with
WebExtensions, does not provide a security boundary, and should not be treated as a
stable public API.

## What works

- Load extensions from a directory on startup (default: `assets/extensions/`).
- Parse a minimal `manifest.json`.
- Run a long-lived background script once at startup, with one QuickJS context per
  extension.
- Use `console.log(...)` inside background scripts.
- Use the current `browser.tabs` subset:
  - `browser.tabs.active()`
  - `browser.tabs.onCreated.addListener(fn)`
  - `browser.tabs.onActivated.addListener(fn)`
  - `browser.tabs.onNavigated.addListener(fn)`, where `fn` receives
    `{ id, url, active }`
- Inject CSS with `browser.scripting.insertCSS({ tabId, cssText })`.
- Enable or disable extensions through the settings file or environment variables.

The bundled `dark-mode` extension injects CSS when tabs are created, activated, or
navigated. Open `https://example.dev/m5` for its demonstration page.

## Current limitations

- No content scripts or direct per-tab DOM access.
- No `removeCSS` API.
- No sandbox or extension security model. Permissions are now *enforced* (see
  below), but that is an API gate, not a security boundary: an extension still
  runs with the browser's own privileges.
- No persistence for extension-owned state across restarts.

## Permissions

As of story 9.4.1 the `permissions` array is enforced, not merely parsed. An API
call from an extension that has not declared the permission it needs is refused
and returns `false`; nothing is thrown, because being refused is an ordinary
answer rather than an error.

| API | Required permission |
| --- | --- |
| `browser.scripting.insertCSS` | `scripting` |

Enforcement works because each extension gets its own QuickJS context and the
host binds that context to the extension's id. Script inside the context cannot
name a different extension, so the identity a native call arrives with is not
forgeable from JS.

A disabled extension has no permissions at all, and an API call carrying an id
that names no loaded extension is refused rather than allowed.

## Directory layout

Extensions live in subdirectories beneath the extension root. The directory name is
the extension ID.

```text
assets/extensions/dark-mode/
  manifest.json
  background.js
```

## Manifest format

Required fields:

- `name`: string
- `version`: string
- `background.entry`: path to the background script, relative to the extension
  directory

The optional `permissions` field is an array of strings.

```json
{
  "name": "Dark Mode",
  "version": "0.1.0",
  "background": { "entry": "background.js" },
  "permissions": ["tabs", "scripting"]
}
```

## Configuration

Startup reads `assets/config/browser.ini`, or the file selected by
`HB_SETTINGS_INI`.

```ini
[extensions]
dark-mode = disabled
```

Accepted values are `enabled|disabled`, `true|false`, `on|off`, `yes|no`, and
`1|0`.

Environment variables take precedence over the settings file:

- `HB_EXTENSIONS_DIR`: override the extensions root.
- `HB_EXTENSIONS_DISABLE`: comma-separated extension IDs to disable.
- `HB_EXTENSIONS_ENABLE`: comma-separated allow-list; when set, only listed IDs
  load.
- `HB_SETTINGS_INI`: override the settings-file path.

Examples:

```bash
HB_EXTENSIONS_DISABLE=dark-mode ./build/Release/Hummingbird
HB_EXTENSIONS_ENABLE=dark-mode,my-ext ./build/Release/Hummingbird
```

The Milestone 5 documentation contains the design and delivery context:
[milestone5.md](milestones/milestone5.md).
