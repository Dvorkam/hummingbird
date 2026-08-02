#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IExtensionApiHost.h"
#include "core/platform_api/IScriptHost.h"

namespace Hummingbird {

namespace DOM {
class Node;
}

struct ScriptEvalResult {
    bool ok = false;
    std::string error;
};

// A DOM event the engine (app/input side) asks the script engine to dispatch to a
// target node through the 7.2.3 propagation pipeline (7.2.4). Platform-agnostic:
// the engine fills the fields, the adapter builds the JS Event.
struct ScriptDomEvent {
    std::string type;
    bool bubbles = true;
    bool cancelable = true;
    std::string key;   // keyboard events
    std::string code;  // keyboard events
};

class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    virtual void bind_host(IScriptHost* host) = 0;
    // `extension_id` names the extension this context belongs to, so the host
    // can tell who is calling and enforce `permissions` (story 9.4.1). One
    // context per extension makes this identity unforgeable from script.
    virtual void bind_extension_host(IExtensionApiHost* host, std::string_view extension_id) = 0;
    virtual ScriptEvalResult eval(std::string_view source, std::string_view filename = "inline") = 0;

    // Drops any per-document state the engine caches across evals (e.g. DOM node
    // wrappers holding raw node pointers). Called on navigation, before the
    // document's arena is reset, so no stale handle survives into the next page.
    virtual void reset_bindings() {}

    // Dispatches `event` to `target` through the DOM event pipeline (capture/
    // target/bubble). Returns false when a listener called preventDefault (the
    // caller should skip the default action). Default: no-op returning true.
    virtual bool dispatch_dom_event(DOM::Node* /*target*/, const ScriptDomEvent& /*event*/) { return true; }

    // Sets the document URL backing window.location (called on navigation/before
    // scripts run). Does not fire hashchange. Default: no-op.
    virtual void set_location(std::string_view /*url*/) {}

    // Same-document fragment navigation (7.2.5): points window.location at `url`
    // and fires `hashchange` when the fragment changed, WITHOUT reloading. Returns
    // true when a hashchange fired. Default: no-op returning false.
    virtual bool navigate_fragment(std::string_view /*url*/) { return false; }

    // Returns and clears the URL a *script* set via `location.hash = ...` since
    // the last consume, so the app can reflect it in the URL bar / tab history
    // (7.7.3). App-initiated navigation (navigate_fragment / set_location) does
    // NOT report here. Default: nothing.
    virtual std::optional<std::string> consume_location_change() { return std::nullopt; }

    // --- History API MVP (9.6.1) ---------------------------------------------
    // What a script asked the session history to do via
    // `history.pushState`/`replaceState`. Reported the same way as a location
    // change: queued by the binding, drained by the Tab, which owns the history
    // stack and the URL bar.
    struct HistoryChange {
        std::string url;    // already resolved against the document's base
        std::string state;  // serialized state as JSON; empty means "no state"
        bool replace = false;
    };
    virtual std::optional<HistoryChange> consume_history_change() { return std::nullopt; }
    // A pending `history.back()`/`forward()`/`go(n)` delta. Traversal is the
    // Tab's job (it owns the stack and the graphics context a re-render needs),
    // so the binding only records the request. Default: nothing.
    virtual std::optional<int> consume_history_delta() { return std::nullopt; }
    // A traversal the page did NOT initiate: back/forward landing on a
    // same-document entry. Sets `location` and `history.state` without a reload,
    // then fires `popstate`. Returns true when the event was dispatched; whether
    // a listener mutated the DOM is reported by the caller that owns the host's
    // mutation epoch, as with hashchange. `state` empty means null.
    virtual bool apply_popstate(std::string_view /*url*/, std::string_view /*state*/) { return false; }
    // How many entries deep the session history is, for `history.length`. The Tab
    // owns the stack, so it has to tell the engine.
    virtual void set_history_length(size_t /*length*/) {}

    // Timer scheduling (7.3.1). `run_due_timers` fires every setTimeout/setInterval
    // callback whose deadline has passed at `now_ms` — a monotonically
    // non-decreasing, document-relative clock in milliseconds — in deadline then
    // registration order; it returns true if any callback ran. `has_pending_timers`
    // reports whether any timer is still scheduled, so the driver knows to keep
    // ticking. Timers are per-document and are dropped by reset_bindings. Defaults:
    // no-op.
    virtual bool run_due_timers(double /*now_ms*/) { return false; }
    virtual bool has_pending_timers() const { return false; }

    // requestAnimationFrame (7.3.3). run_animation_frames fires every callback
    // registered as of this frame exactly once, passing `now_ms` as the frame
    // timestamp; callbacks that re-request run on the *next* frame (so rAF-driven
    // animation does not grow the queue). Returns true if any callback ran.
    // has_pending_animation_frames keeps the driver ticking. Per-document; dropped
    // by reset_bindings. Defaults: no-op.
    virtual bool run_animation_frames(double /*now_ms*/) { return false; }
    virtual bool has_pending_animation_frames() const { return false; }

    // Missing-API telemetry (7.5.2): names of unimplemented JS APIs the current
    // page touched, deduped and in first-touch order. Fail-soft — touching such
    // an API logs once and no-ops rather than throwing, so the rest of the script
    // still runs. Reset per document. Empty by default.
    virtual std::vector<std::string> missing_apis() const { return {}; }

    // fetch (9.1.1). The binding hands JS a Promise and remembers how to settle
    // it; `settle_fetch` is how the answer gets back in. It MUST be called on the
    // engine's own thread — the driver queues transport results and drains them
    // during its ordinary tick — and never after reset_bindings, which drops
    // every pending request so nothing settles into a torn-down document.
    // Returns false if the id is unknown (already settled, or cancelled by a
    // navigation that raced the response). Default: no-op.
    virtual bool settle_fetch(const ScriptFetchResponse& /*response*/) { return false; }
    // How many requests are still waiting, so a driver can keep ticking while
    // any are outstanding. Default: none.
    virtual size_t pending_fetch_count() const { return 0; }
};

using ScriptEnginePtr = std::unique_ptr<IScriptEngine>;

}  // namespace Hummingbird
