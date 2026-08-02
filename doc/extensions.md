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
- Block network requests declaratively (story 9.4.1), either from a static
  ruleset named in the manifest or from
  `browser.declarativeRequest.updateRules({ rules: [...] })`. See
  [Request filtering](#request-filtering) below.

The bundled `dark-mode` extension injects CSS when tabs are created, activated, or
navigated. Open `https://example.dev/m5` for its demonstration page.

The bundled `ad-block-lite` extension blocks a curated list of third-party
tracker domains. Open `https://example.dev/m9-adblock` for its demonstration
page. Its `background.js` is deliberately almost empty — the rules live in
`rules.json` and are matched natively, so a correct blocker needs no code.

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
| `browser.declarativeRequest.updateRules` | `declarativeRequest` |
| `declarative_net_request.rule_resources` (manifest) | `declarativeRequest` |

Enforcement works because each extension gets its own QuickJS context and the
host binds that context to the extension's id. Script inside the context cannot
name a different extension, so the identity a native call arrives with is not
forgeable from JS.

A disabled extension has no permissions at all, and an API call carrying an id
that names no loaded extension is refused rather than allowed.

## Request filtering

An extension can declare requests the engine should refuse. Rules are matched
natively in C++ at the network choke point, so **no extension JavaScript runs on
the request path** — a declarative rule cannot hang a request, and matching is
testable without a script engine.

Rules come from two places, and they are independent: a dynamic update does not
disturb the manifest ruleset.

**Static rulesets** are named in the manifest, read by the host at startup, and
therefore survive a restart with no persistence involved — they are simply read
again. Prefer these.

```json
{
  "name": "Ad Block Lite",
  "version": "0.1.0",
  "background": { "entry": "background.js" },
  "permissions": ["declarativeRequest"],
  "declarative_net_request": { "rule_resources": ["rules.json"] }
}
```

**Dynamic rules** come from the background script and are **session-scoped**:
they are not persisted, and are dropped when the extension is disabled or the
browser closes.

```js
browser.declarativeRequest.updateRules({
  rules: [
    { id: 1,
      condition: { requestDomains: ["tracker.example"] },
      action: { type: "block" } }
  ]
});
```

### Rule format

The shape follows MV3's `declarativeNetRequest`, trimmed to what the matcher
supports. A rule needs `urlFilter`, `requestDomains`, or both.

| Field | Meaning |
| --- | --- |
| `id` | Identifies the rule in logs. |
| `condition.urlFilter` | Substring the URL must contain, matched case-insensitively against the full URL. |
| `condition.requestDomains` | One domain; matches it and its subdomains (`doubleclick.net` matches `ad.doubleclick.net`, not `notdoubleclick.net`). |
| `condition.resourceTypes` | Any of `stylesheet`, `image`, `font`, `script`, `xmlhttprequest`. Omit for all of them. |
| `condition.domainType` | `thirdParty` or `firstParty`, compared by registrable domain. |
| `action.type` | Must be `block`. |

### Deliberate limits

- **Block only.** No `allow` (exception) rules, no redirect, no header
  modification. `action.type` is still required so that adding those later
  cannot silently reinterpret rules already written.
- **Top-level navigations are never blocked.** `main_frame` parses but never
  matches: blocking a navigation needs an interstitial page that does not exist
  yet, and without one it would render as a network error or a blank tab.
- **One domain per rule.** A `requestDomains` list with more than one entry is
  rejected rather than truncated.
- **Curated lists only.** Matching is a linear scan, which is honest for tens of
  rules and wrong for tens of thousands. EasyList-scale lists need an indexed
  matcher — see `T-EXT-EASYLIST-1`.
- A blocked resource reaches the state `Blocked`, distinct from `Failed`, so it
  does not trigger error pages or get re-requested.

## Directory layout

Extensions live in subdirectories beneath the extension root. The directory name is
the extension ID.

```text
assets/extensions/dark-mode/
  manifest.json
  background.js

assets/extensions/ad-block-lite/
  manifest.json
  background.js
  rules.json
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
