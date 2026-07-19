#pragma once

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "engine/document/ExternalScriptLookup.h"
#include "engine/forms/FormSubmission.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IResourceProvider;
class IGraphicsContext;
class IImageDecoder;
class IScriptEngine;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::DOM {
class Element;
class Node;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Layout {
class RenderObject;
}  // namespace Hummingbird::Layout

namespace Hummingbird::Engine {

class ResourceStore;
class DocumentInteraction;
class DocumentModel;
class DocumentRenderer;
class DocumentResources;
class DocumentStyleCoordinator;
class DocumentScripting;

class DocumentPipeline {
public:
    struct HitTestContext {
        Layout::Point point;
        Layout::Rect viewport;
        std::string_view base_url;
        float scroll_y = 0.0f;
        int click_count = 1;  // 2 on a double-click (drives the dblclick event)
    };

    struct PaintContext {
        Layout::Rect viewport;
        bool debug_outlines = false;
        float scroll_y = 0.0f;
    };

    struct InputEditResult {
        bool handled = false;
        bool needs_repaint = false;
        bool mutated = false;  // a JS key listener mutated the DOM (caller must rebuild)
        std::optional<FormSubmission> submitted_form;
    };

    struct ScriptDispatchResult {
        bool handled = false;
        bool mutated = false;
        bool default_prevented = false;
    };

    DocumentPipeline(ResourceStore* resource_store, IResourceProvider* resource_provider, IImageDecoder* image_decoder,
                     std::unique_ptr<IScriptEngine> script_engine);
    ~DocumentPipeline();

    DocumentPipeline(const DocumentPipeline&) = delete;
    DocumentPipeline& operator=(const DocumentPipeline&) = delete;
    DocumentPipeline(DocumentPipeline&&) = delete;
    DocumentPipeline& operator=(DocumentPipeline&&) = delete;

    void reset();

    // --- document build + layout ---
    bool parse_html(std::string_view html);
    // Runs all <script>s in document order; external bodies come from the
    // lookup (see DocumentScripting). Returns true when scripts mutated the DOM.
    bool run_scripts(const ExternalScriptLookup& external_lookup = {});
    // Raw src attributes of external <script>s in document order (for the
    // caller to request through the resource loader before running).
    std::vector<std::string> external_script_srcs() const;
    void set_extension_style_blocks(const std::vector<std::string>& style_blocks);
    void apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport, std::string_view base_url);
    bool rebuild_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport, std::string_view base_url);
    bool update_image_resources(std::string_view base_url);
    // Record a URL as visited so anchors pointing at it style with `:visited`
    // (T-HIST-1). The set persists across navigations within the tab.
    void mark_url_visited(std::string_view url);
    // True when the new viewport flips any @media rule vs. the last style
    // application — the caller must restyle, a plain relayout is not enough.
    bool needs_restyle_for_viewport(const Layout::Rect& viewport) const;
    void relayout(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void paint(IGraphicsContext& graphics, const PaintContext& context);
    void paint_controls(IGraphicsContext& graphics, const PaintContext& context, bool repaint_background);
    bool has_dom_tree() const;
    bool has_render_tree() const;
    // Root of the current render tree (read-only). Lets tests/inspection locate a
    // laid-out element's box — e.g. to synthesize a click at its coordinates.
    const Layout::RenderObject* render_root() const;
    float content_height() const;
    size_t render_tree_children() const;
    const std::vector<std::string>& stylesheet_links() const;
    const std::vector<std::string>& image_links() const;
    const std::vector<std::string>& background_image_links() const;
    const std::vector<std::string>& font_requests() const;

    // --- interaction ---
    ScriptDispatchResult dispatch_click(const HitTestContext& context);
    ScriptDispatchResult dispatch_load();
    std::optional<std::string> hit_test_link(const HitTestContext& context) const;
    std::optional<std::string> inspect_at(const HitTestContext& context) const;
    std::optional<FormSubmission> submit_form_at(const HitTestContext& context) const;
    bool focus_input_at(const HitTestContext& context);
    bool focus_autofocus_input();
    bool clear_input_focus();
    bool set_control_interaction_at(const HitTestContext& context);
    bool clear_control_interaction();
    bool has_focused_input() const;
    InputEditResult handle_text_input(std::string_view text);
    InputEditResult handle_key_down(const InputEvent& event, std::string_view base_url);
    InputEditResult handle_key_up(const InputEvent& event);
    std::optional<std::string> focused_input_value() const;

    struct SubmitDispatchResult {
        bool default_prevented = false;  // a submit listener called preventDefault
        bool mutated = false;
    };
    // Fires a cancelable DOM `submit` on the form before it navigates (7.2.4.4).
    SubmitDispatchResult dispatch_submit(const DOM::Element* form);

    // window.location / fragment navigation (7.2.5).
    struct FragmentNavResult {
        bool hash_changed = false;  // the fragment differed → hashchange fired
        bool mutated = false;       // a hashchange listener changed the DOM
    };
    void set_location(std::string_view url);
    FragmentNavResult navigate_fragment(std::string_view url);

    // Count of completed style+layout passes (7.4.1 invalidation instrumentation).
    // Batching guarantee: a task making N DOM mutations advances this by exactly 1.
    size_t style_layout_pass_count() const { return style_layout_passes_; }

    // Timers (7.3.1).
    struct TimerRunResult {
        bool fired = false;    // at least one timer callback ran
        bool mutated = false;  // a callback changed the DOM (caller must rebuild)
    };
    // Fires due setTimeout/setInterval callbacks on the document-relative clock.
    TimerRunResult run_timers(double now_ms);
    bool has_pending_timers() const;

private:
    struct KeyDispatchResult {
        bool mutated = false;
        bool default_prevented = false;
    };
    // Dispatches a DOM keyboard event (`type`) to the focused element / document.
    KeyDispatchResult dispatch_key_event(const char* type, const InputEvent& event);
    // Dispatches a fieldless DOM event (`type`) to `target` (input/change/focus/blur).
    KeyDispatchResult dispatch_element_event(DOM::Node* target, const char* type, bool bubbles, bool cancelable);
    // On a focus move `before`→`after` (either may be null): fires `change` on
    // `before` when its value changed since focus, then `blur` on `before` and
    // `focus` on `after`, snapshotting the newly-focused value.
    bool fire_focus_transition(DOM::Element* before, DOM::Element* after);

    std::string focus_snapshot_value_;  // focused input's value when it gained focus

    std::unique_ptr<DocumentResources> resources_;
    std::unique_ptr<DocumentModel> model_;
    std::unique_ptr<DocumentInteraction> interaction_;
    std::unique_ptr<DocumentRenderer> renderer_;
    std::unique_ptr<DocumentStyleCoordinator> style_coordinator_;
    std::unique_ptr<DocumentScripting> scripting_;
    std::unordered_set<std::string> visited_urls_;
    size_t style_layout_passes_ = 0;  // 7.4.1 invalidation budget instrumentation
};

}  // namespace Hummingbird::Engine
