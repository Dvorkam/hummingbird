#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/SecurityState.h"
#include "engine/forms/FormSubmission.h"
#include "engine/resources/ResourceLoader.h"
#include "engine/resources/ResourceStore.h"
#include "engine/tab/NavigationLifecycle.h"
#include "engine/tab/TabAnimationTicker.h"
#include "engine/tab/TabLayoutState.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
class IImageDecoder;
class INetwork;
class IResourceProvider;
class IScriptEngine;
enum class NetworkError;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::Engine {
class DocumentPipeline;
class ResourceLoader;
class Tab {
public:
    struct KeyResult {
        bool handled = false;
        bool needs_repaint = false;
        bool mutated = false;  // a JS key listener changed the DOM (document repainted)
        std::optional<FormSubmission> submitted_form;
    };
    struct ClickResult {
        bool handled = false;
        bool mutated = false;
        bool default_prevented = false;  // a JS click listener called preventDefault
    };
    struct SubmitResult {
        bool default_prevented = false;  // a JS submit listener called preventDefault
        bool mutated = false;
    };
    struct FragmentResult {
        bool hash_changed = false;
        bool mutated = false;
    };
    Tab(std::unique_ptr<INetwork> network, std::unique_ptr<INetwork> fallback_network,
        std::unique_ptr<IResourceProvider> resource_provider, std::unique_ptr<IImageDecoder> image_decoder,
        std::unique_ptr<IScriptEngine> script_engine);
    ~Tab();

    Tab(const Tab&) = delete;
    Tab& operator=(const Tab&) = delete;
    Tab(Tab&&) = delete;
    Tab& operator=(Tab&&) = delete;

    void shutdown();

    void navigate(std::string_view url);
    void navigate(const FormSubmission& submission);

    // Processes pending navigation results and keeps layout in sync with the viewport.
    // Returns true if the document changed in a way that needs repainting.
    bool tick(IGraphicsContext& graphics, const Layout::Rect& viewport);

    // Paints the current document into the given viewport using the current scroll offset.
    void paint(IGraphicsContext& graphics, const Layout::Rect& viewport, bool debug_outlines);
    // Paints just the input controls without re-drawing the full document.
    void paint_controls(IGraphicsContext& graphics, const Layout::Rect& viewport);

    // --- document interaction ---
    // Returns a resolved link URL for the render node under the window-space point.
    std::optional<std::string> hit_test_link(const Layout::Point& point, const Layout::Rect& viewport) const;
    // F1 debug inspection: describe the topmost element under the point
    // (T-DEBUG-INSPECT-1).
    std::optional<std::string> inspect_at(const Layout::Point& point, const Layout::Rect& viewport) const;
    ClickResult dispatch_click(const Layout::Point& point, const Layout::Rect& viewport, IGraphicsContext& graphics,
                               int click_count = 1);
    std::optional<FormSubmission> submit_form_at(const Layout::Point& point, const Layout::Rect& viewport) const;
    bool focus_input_at(const Layout::Point& point, const Layout::Rect& viewport);
    bool clear_input_focus();
    bool set_control_interaction_at(const Layout::Point& point, const Layout::Rect& viewport);
    bool clear_control_interaction();
    bool refresh_styles_for_interaction(IGraphicsContext& graphics, const Layout::Rect& viewport);
    bool has_focused_input() const;
    bool handle_text_input(std::string_view text);
    KeyResult handle_key_down(const InputEvent& event);
    KeyResult handle_key_up(const InputEvent& event);
    SubmitResult dispatch_submit(const DOM::Element* form);
    // Same-document fragment navigation (7.2.5): fires hashchange without a
    // reload; rebuilds if a listener mutated the DOM.
    FragmentResult navigate_fragment(std::string_view url, IGraphicsContext& graphics, const Layout::Rect& viewport);
    std::optional<std::string> focused_input_value() const;

    std::optional<std::string> consume_navigation_commit_url();
    bool insert_extension_css(std::string_view css_text);

    void scroll_by(float delta_px, float viewport_height);

    float scroll_y() const { return layout_state_.scroll_y; }
    float content_height() const { return layout_state_.content_height; }
    std::string_view requested_url() const { return navigation_lifecycle_.requested_url(); }
    SecurityState security_state() const { return navigation_lifecycle_.security_state(); }
    std::optional<ResourceView> resource_view(std::string_view url, ResourceType type) const;

    bool allow_insecure_for_current_host();

private:
    void consume_pending_resources(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void process_incremental_resource_updates(const ResourceLoader::BatchResult& batch, IGraphicsContext& graphics,
                                              const Layout::Rect& viewport);
    void sync_extension_styles_before_stylesheet_update();
    void handle_document_ready(std::string_view document_url, std::string_view effective_url,
                               NetworkError document_error, IGraphicsContext& graphics, const Layout::Rect& viewport);
    std::optional<std::string_view> resolve_document_ready_body(std::string_view document_url) const;
    void rebuild_after_document_ready(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void handle_stylesheet_ready(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void handle_image_ready(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void apply_extension_css_if_needed(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void relayout_if_viewport_changed(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void process_animation_updates();
    bool rebuild_document_and_sync_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                          std::string_view reason);
    void begin_navigation_session(std::string_view url);
    bool prepare_document_from_response(std::string_view html);
    void apply_load_mutations_after_document_ready(IGraphicsContext& graphics, const Layout::Rect& viewport);
    // External <script src> gating (7.0.1): scripts run once every external
    // body is Ready or Failed, then the load event dispatches.
    void maybe_run_deferred_scripts(IGraphicsContext& graphics, const Layout::Rect& viewport);
    bool all_external_scripts_resolved() const;
    bool run_document_scripts_now();
    void apply_autofocus_after_rebuild();
    void mark_dirty(std::string_view reason = {});
    void reset_document_state();
    void update_layout_state(const Layout::Rect& viewport, std::string_view reason);
    bool advance_animation_tick();

private:
    std::atomic<bool> shutting_down_{false};

    std::unique_ptr<ResourceLoader> resource_loader_;
    std::unique_ptr<DocumentPipeline> document_pipeline_;
    std::vector<std::string> extension_style_blocks_;
    std::unordered_set<std::string> extension_style_block_keys_;
    bool extension_css_dirty_ = false;
    // True while external <script src> fetches block script execution and the
    // load event (cleared by maybe_run_deferred_scripts or navigation reset).
    bool scripts_pending_ = false;
    NavigationLifecycle navigation_lifecycle_{};
    TabLayoutState layout_state_{};
    TabAnimationTicker animation_ticker_{};

    bool dirty_ = true;
};

}  // namespace Hummingbird::Engine
