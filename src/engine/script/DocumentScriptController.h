#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IScriptEngine.h"
#include "core/platform_api/ScriptFetch.h"
#include "engine/script/DocumentScriptHost.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::Core {
class ArenaAllocator;
class StorageArea;
}  // namespace Hummingbird::Core

namespace Hummingbird::DOM {
class Node;
class Element;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentScriptController final {
public:
    struct ScriptDispatchResult {
        bool handled = false;
        bool mutated = false;
        bool default_prevented = false;  // a listener called preventDefault
    };

    // One script body ready to eval; views must stay valid for the run_scripts
    // call (they point into the document model / resource store).
    struct ScriptSource {
        std::string_view text;
        std::string_view context_name;
    };

    explicit DocumentScriptController(ScriptEnginePtr engine);

    void clear();
    // Routes JS focus()/blur() to the caret target (wired by the pipeline).
    void set_focus_sink(std::function<void(DOM::Element*, bool)> sink);
    void set_cookie_accessors(std::function<std::string()> reader, std::function<void(std::string_view)> writer);
    void set_storage_accessor(std::function<Core::StorageArea*()> accessor);
    // fetch (9.1.1): supplied by the Tab, which owns the loader and the URL.
    void set_fetch_sink(std::function<std::uint64_t(const ScriptFetchRequest&)> sink);
    void set_url_resolver(std::function<std::string(std::string_view)> resolver);
    // Settles one fetch and reports whether its continuation mutated the DOM.
    bool settle_fetch(DOM::Node* dom_root, Core::ArenaAllocator* arena, const ScriptFetchResponse& response);
    void set_session_storage_accessor(std::function<Core::StorageArea*()> accessor);

    bool run_scripts(const std::vector<ScriptSource>& scripts, DOM::Node* dom_root, Core::ArenaAllocator* arena);
    ScriptDispatchResult dispatch_click(DOM::Node* dom_root, Core::ArenaAllocator* arena,
                                        const Layout::RenderObject* render_tree, const Layout::Rect& viewport,
                                        const Layout::Point& point, float scroll_y, int click_count = 1);
    ScriptDispatchResult dispatch_load(DOM::Node* dom_root, Core::ArenaAllocator* arena);
    // window.location / fragment navigation (7.2.5).
    void set_location(std::string_view url);
    ScriptDispatchResult navigate_fragment(DOM::Node* dom_root, Core::ArenaAllocator* arena, std::string_view url);
    // Dispatches an already-built DOM event to `target` (keyboard/input/etc.);
    // the caller decides the target node and event fields.
    ScriptDispatchResult dispatch_dom_event(DOM::Node* dom_root, Core::ArenaAllocator* arena, DOM::Node* target,
                                            const ScriptDomEvent& event);
    // Fires every timer whose deadline has passed at `now_ms` (7.3.1). `handled`
    // is true when at least one callback ran; `mutated` when the DOM changed.
    ScriptDispatchResult run_timers(DOM::Node* dom_root, Core::ArenaAllocator* arena, double now_ms);
    // True while a timer is still scheduled, so the tab keeps ticking.
    bool has_pending_timers() const;
    // Fires this frame's requestAnimationFrame callbacks (7.3.3).
    ScriptDispatchResult run_animation_frames(DOM::Node* dom_root, Core::ArenaAllocator* arena, double now_ms);
    bool has_pending_animation_frames() const;
    // Returns/clears a script-initiated location.hash change to reflect in the
    // chrome + tab history (7.7.3).
    std::optional<std::string> consume_location_change();
    // History API MVP (9.6.1): pass-throughs plus the popstate dispatch, which
    // reports DOM mutation the same way navigate_fragment does.
    std::optional<IScriptEngine::HistoryChange> consume_history_change();
    std::optional<int> consume_history_delta();
    void set_history_length(size_t length);
    ScriptDispatchResult apply_popstate(DOM::Node* dom_root, Core::ArenaAllocator* arena, std::string_view url,
                                        std::string_view state);

    // Unimplemented APIs this document's scripts touched (7.5.2 telemetry).
    // Read at document end, before reset() clears it.
    std::vector<std::string> missing_apis() const {
        return script_engine_ ? script_engine_->missing_apis() : std::vector<std::string>{};
    }

private:
    bool bind_host(DOM::Node* dom_root, Core::ArenaAllocator* arena);
    ScriptDispatchResult eval_inline_script(DOM::Node* dom_root, Core::ArenaAllocator* arena, std::string_view script,
                                            std::string_view context_name);

    ScriptEnginePtr script_engine_;
    DocumentScriptHost script_host_;
};

}  // namespace Hummingbird::Engine
