#include "engine/tab/Tab.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <utility>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IScriptEngine.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "core/utils/Url.h"
#include "engine/document/DocumentPipeline.h"
#include "engine/resources/ResourceLoader.h"
#include "engine/resources/ResourceRequestPlanner.h"
#include "engine/tab/TabDocumentReadyPolicy.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Engine {

namespace {
constexpr int kAnimationTickMinIntervalMs = 80;

bool tab_dirty_debug_enabled() {
    static const bool enabled = std::getenv("HB_DEBUG_TAB_DIRTY") != nullptr;
    return enabled;
}

bool layout_state_debug_enabled() {
    static const bool enabled = std::getenv("HB_DEBUG_LAYOUT_STATE") != nullptr;
    return enabled;
}

void maybe_log_tab_dirty(std::string_view reason, bool dirty) {
    if (!dirty || !tab_dirty_debug_enabled()) {
        return;
    }
    static int log_count = 0;
    ++log_count;
    if (log_count <= 20 || (log_count % 120) == 0) {
        HB_LOG_WARN("[tab-debug] dirty reason=" << reason << " count=" << log_count);
    }
}

DocumentPipeline::HitTestContext make_hit_test_context(const Layout::Point& point, const Layout::Rect& viewport,
                                                       std::string_view base_url, float scroll_y) {
    return {point, viewport, base_url, scroll_y};
}
}  // namespace

Tab::Tab(std::unique_ptr<INetwork> network, std::unique_ptr<INetwork> fallback_network,
         std::unique_ptr<IResourceProvider> resource_provider, std::unique_ptr<IImageDecoder> image_decoder,
         std::unique_ptr<IScriptEngine> script_engine, std::shared_ptr<Core::CookieJar> cookie_jar)
    : resource_loader_(std::make_unique<ResourceLoader>(std::move(network), std::move(fallback_network),
                                                        std::move(resource_provider), std::move(image_decoder),
                                                        std::move(cookie_jar))),
      document_pipeline_(
          std::make_unique<DocumentPipeline>(&resource_loader_->store(), resource_loader_->resource_provider(),
                                             resource_loader_->image_decoder(), std::move(script_engine))) {}

Tab::~Tab() {
    shutdown();
}

void Tab::shutdown() {
    if (shutting_down_.exchange(true, std::memory_order_relaxed)) return;
    resource_loader_->shutdown();
}

std::string Tab::initiator_host_for(NavigationSource source) const {
    if (source != NavigationSource::Document) {
        return {};
    }
    // The document currently loaded is the initiator. Must be read BEFORE
    // begin_navigation_session, which switches requested_url() to the target.
    if (auto parts = Core::parse_absolute_url(navigation_lifecycle_.requested_url())) {
        return parts->host;
    }
    return {};
}

void Tab::navigate(std::string_view url, NavigationSource source) {
    if (shutting_down_.load(std::memory_order_relaxed)) return;

    ResourceLoader::DocumentRequest request{};
    request.initiator_host = initiator_host_for(source);

    begin_navigation_session(url);
    if (!in_history_navigation_) {
        history_.push(std::string(navigation_lifecycle_.requested_url()));
    }
    resource_loader_->navigate(navigation_lifecycle_.requested_url(), request);
}

bool Tab::go_back(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (shutting_down_.load(std::memory_order_relaxed) || !history_.can_go_back()) return false;
    navigate_history_entry(history_.go_back(), graphics, viewport);
    return true;
}

bool Tab::go_forward(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (shutting_down_.load(std::memory_order_relaxed) || !history_.can_go_forward()) return false;
    navigate_history_entry(history_.go_forward(), graphics, viewport);
    return true;
}

void Tab::navigate_history_entry(const std::string& url, IGraphicsContext& graphics, const Layout::Rect& viewport) {
    in_history_navigation_ = true;
    const std::string_view current = navigation_lifecycle_.requested_url();
    if (Core::url_without_fragment(url) == Core::url_without_fragment(current) &&
        Core::url_fragment(url) != Core::url_fragment(current)) {
        // Same document, only the fragment differs: navigate in place (no reload).
        (void)navigate_fragment(url, graphics, viewport);
    } else {
        navigate(url);  // different document (or exact reload): full (re)load
    }
    in_history_navigation_ = false;
}

void Tab::navigate(const FormSubmission& submission) {
    if (shutting_down_.load(std::memory_order_relaxed)) return;

    ResourceLoader::DocumentRequest request{};
    // A submit always comes from the loaded document. Captured before the
    // session switches to the target URL.
    request.initiator_host = initiator_host_for(NavigationSource::Document);

    begin_navigation_session(submission.url);
    if (!in_history_navigation_) {
        history_.push(std::string(navigation_lifecycle_.requested_url()));
    }

    if (submission.method == FormSubmitMethod::Post) {
        request.method = ResourceLoader::DocumentRequest::Method::Post;
        request.body = submission.body;
        request.content_type = submission.content_type;
    }
    resource_loader_->navigate(navigation_lifecycle_.requested_url(), request);
}

bool Tab::tick(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (shutting_down_.load(std::memory_order_relaxed)) return false;

    consume_pending_resources(graphics, viewport);
    apply_extension_css_if_needed(graphics, viewport);
    relayout_if_viewport_changed(graphics, viewport);
    process_animation_updates();
    process_scheduled_scripts(graphics, viewport);
    process_script_url_change();

    bool dirty = dirty_;
    dirty_ = false;
    return dirty;
}

void Tab::apply_extension_css_if_needed(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (extension_css_dirty_ && document_pipeline_->has_dom_tree()) {
        document_pipeline_->set_extension_style_blocks(extension_style_blocks_);
        (void)rebuild_document_and_sync_layout(graphics, viewport, "tick:extension_css");
        extension_css_dirty_ = false;
        mark_dirty("extension_css");
    }
}

void Tab::relayout_if_viewport_changed(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (document_pipeline_->has_render_tree()) {
        if (layout_state_.viewport_changed(viewport)) {
            if (document_pipeline_->needs_restyle_for_viewport(viewport)) {
                // Crossing a @media breakpoint changes which rules apply;
                // a plain relayout would keep the old styles (T-MEDIA-RESIZE-1).
                (void)rebuild_document_and_sync_layout(graphics, viewport, "tick:viewport_breakpoint");
            } else {
                document_pipeline_->relayout(graphics, viewport);
                update_layout_state(viewport, "tick:viewport_changed");
            }
            mark_dirty("viewport_changed");
        }
    }
}

void Tab::process_animation_updates() {
    if (advance_animation_tick()) {
        mark_dirty("animation_frame_advanced");
    }
}

bool Tab::advance_animation_tick() {
    auto ready_delta_ms = animation_ticker_.consume_ready_delta_ms(kAnimationTickMinIntervalMs);
    if (!ready_delta_ms.has_value()) {
        return false;
    }

    bool updated = false;
    if (resource_loader_->store().tick_animations(*ready_delta_ms) && document_pipeline_->has_render_tree()) {
        updated = document_pipeline_->update_image_resources(navigation_lifecycle_.requested_url());
    }
    return updated;
}

void Tab::process_scheduled_scripts(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    const auto now = Core::Clock::now();
    const bool active = document_pipeline_->has_dom_tree() && (document_pipeline_->has_pending_timers() ||
                                                               document_pipeline_->has_pending_animation_frames());
    if (!timer_has_tick_ || !active) {
        // First tick, or idle: hold the clock and keep the baseline current so a
        // newly scheduled timer/frame measures its delay from now, not document load.
        timer_last_tick_ = now;
        timer_has_tick_ = true;
        if (!active) return;
    }
    timer_clock_ms_ += Core::duration_ms(timer_last_tick_, now);
    timer_last_tick_ = now;

    bool mutated = false;
    // requestAnimationFrame callbacks run once per frame before paint; timers are
    // ordinary tasks. Run the frame callbacks first, then any due timers.
    mutated |= document_pipeline_->run_animation_frames(timer_clock_ms_).mutated;
    mutated |= document_pipeline_->run_timers(timer_clock_ms_).mutated;
    if (mutated) {
        (void)rebuild_document_and_sync_layout(graphics, viewport, "tick:scheduled_script_mutation");
        mark_dirty("scheduled_script_mutation");
    }
}

size_t Tab::style_layout_pass_count() const {
    return document_pipeline_->style_layout_pass_count();
}

void Tab::process_script_url_change() {
    // A script assigned location.hash (7.7.3): reflect it in the tab's requested
    // URL in place (no reload) and queue the URL-bar text for the app.
    if (auto url = document_pipeline_->consume_location_change()) {
        navigation_lifecycle_.update_fragment_url(*url);
        if (!in_history_navigation_) {
            history_.push(*url);  // JS hash routing is a history entry too (7.6.1)
        }
        pending_url_bar_update_ = std::move(url);
        mark_dirty("script_location_change");
    }
}

std::optional<std::string> Tab::consume_url_bar_update() {
    std::optional<std::string> out = std::move(pending_url_bar_update_);
    pending_url_bar_update_.reset();
    return out;
}

void Tab::paint(IGraphicsContext& graphics, const Layout::Rect& viewport, bool debug_outlines) {
    if (!document_pipeline_->has_render_tree()) return;
    graphics.set_text_cache_owner(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(this)));
    DocumentPipeline::PaintContext context{viewport, debug_outlines, layout_state_.scroll_y};
    document_pipeline_->paint(graphics, context);
    graphics.set_text_cache_owner(0);
}

void Tab::paint_controls(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (!document_pipeline_->has_render_tree()) return;
    graphics.set_text_cache_owner(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(this)));
    DocumentPipeline::PaintContext context{viewport, false, layout_state_.scroll_y};
    document_pipeline_->paint_controls(graphics, context, true);
    graphics.set_text_cache_owner(0);
}

std::optional<std::string> Tab::hit_test_link(const Layout::Point& point, const Layout::Rect& viewport) const {
    return document_pipeline_->hit_test_link(
        make_hit_test_context(point, viewport, navigation_lifecycle_.requested_url(), layout_state_.scroll_y));
}

std::optional<std::string> Tab::inspect_at(const Layout::Point& point, const Layout::Rect& viewport) const {
    return document_pipeline_->inspect_at(
        make_hit_test_context(point, viewport, navigation_lifecycle_.requested_url(), layout_state_.scroll_y));
}

Tab::ClickResult Tab::dispatch_click(const Layout::Point& point, const Layout::Rect& viewport,
                                     IGraphicsContext& graphics, int click_count) {
    auto context =
        make_hit_test_context(point, viewport, navigation_lifecycle_.requested_url(), layout_state_.scroll_y);
    context.click_count = click_count;
    auto result = document_pipeline_->dispatch_click(context);
    if (result.mutated) {
        (void)rebuild_document_and_sync_layout(graphics, viewport, "dispatch_click:script_mutation");
        mark_dirty();
    }
    return {result.handled, result.mutated, result.default_prevented};
}

std::optional<FormSubmission> Tab::submit_form_at(const Layout::Point& point, const Layout::Rect& viewport) const {
    return document_pipeline_->submit_form_at(
        make_hit_test_context(point, viewport, navigation_lifecycle_.requested_url(), layout_state_.scroll_y));
}

bool Tab::focus_input_at(const Layout::Point& point, const Layout::Rect& viewport) {
    return document_pipeline_->focus_input_at(
        make_hit_test_context(point, viewport, navigation_lifecycle_.requested_url(), layout_state_.scroll_y));
}

bool Tab::clear_input_focus() {
    return document_pipeline_->clear_input_focus();
}

bool Tab::set_control_interaction_at(const Layout::Point& point, const Layout::Rect& viewport) {
    return document_pipeline_->set_control_interaction_at(
        make_hit_test_context(point, viewport, navigation_lifecycle_.requested_url(), layout_state_.scroll_y));
}

bool Tab::clear_control_interaction() {
    return document_pipeline_->clear_control_interaction();
}

bool Tab::refresh_styles_for_interaction(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (!document_pipeline_->has_dom_tree()) {
        return false;
    }
    return rebuild_document_and_sync_layout(graphics, viewport, "refresh_styles_for_interaction");
}

bool Tab::has_focused_input() const {
    return document_pipeline_->has_focused_input();
}

bool Tab::handle_text_input(std::string_view text) {
    auto result = document_pipeline_->handle_text_input(text);
    return result.handled;
}

Tab::KeyResult Tab::handle_key_down(const InputEvent& event) {
    auto result = document_pipeline_->handle_key_down(event, navigation_lifecycle_.requested_url());
    return {result.handled, result.needs_repaint, result.mutated, std::move(result.submitted_form)};
}

Tab::KeyResult Tab::handle_key_up(const InputEvent& event) {
    auto result = document_pipeline_->handle_key_up(event);
    // keyup has no default action; a listener may still have mutated the DOM.
    return {result.mutated, result.needs_repaint, result.mutated, std::move(result.submitted_form)};
}

Tab::SubmitResult Tab::dispatch_submit(const DOM::Element* form) {
    auto result = document_pipeline_->dispatch_submit(form);
    return {result.default_prevented, result.mutated};
}

Tab::FragmentResult Tab::navigate_fragment(std::string_view url, IGraphicsContext& graphics,
                                           const Layout::Rect& viewport) {
    auto result = document_pipeline_->navigate_fragment(url);
    if (result.hash_changed) {
        // Same-document fragment nav: keep the tab's requested URL in sync so
        // back/forward history and the URL bar reflect it (7.7.3 mirror of the
        // click path's URL-bar update).
        navigation_lifecycle_.update_fragment_url(url);
        if (!in_history_navigation_) {
            history_.push(std::string(url));  // fragment routes are history entries (7.6.1)
        }
    }
    if (result.mutated) {
        (void)rebuild_document_and_sync_layout(graphics, viewport, "navigate_fragment:hashchange_mutation");
        mark_dirty();
    }
    return {result.hash_changed, result.mutated};
}

std::optional<std::string> Tab::focused_input_value() const {
    return document_pipeline_->focused_input_value();
}

std::optional<std::string> Tab::consume_navigation_commit_url() {
    return navigation_lifecycle_.consume_pending_commit_url();
}

bool Tab::insert_extension_css(std::string_view css_text) {
    if (css_text.empty()) {
        return false;
    }
    auto [it, inserted] = extension_style_block_keys_.insert(std::string(css_text));
    if (!inserted) {
        return true;
    }
    extension_style_blocks_.push_back(*it);
    extension_css_dirty_ = true;
    mark_dirty();
    return true;
}

std::optional<ResourceView> Tab::resource_view(std::string_view url, ResourceType type) const {
    return resource_loader_->view(url, type);
}

void Tab::scroll_by(float delta_px, float viewport_height) {
    layout_state_.scroll_by(delta_px, viewport_height);
    mark_dirty();
    if (tab_dirty_debug_enabled()) {
        HB_LOG_WARN("[tab-debug] scroll_by delta_px=" << delta_px << " viewport_h=" << viewport_height
                                                      << " new_scroll_y=" << layout_state_.scroll_y
                                                      << " content_h=" << layout_state_.content_height);
    }
}

void Tab::consume_pending_resources(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    auto result = resource_loader_->consume_pending_updates();
    if (result.pending_count == 0) return;

    if (result.is_ready(ResourceType::Document)) {
        handle_document_ready(result.document_url, result.effective_url, result.document_error, graphics, viewport);
        return;
    }
    process_incremental_resource_updates(result, graphics, viewport);
}

void Tab::process_incremental_resource_updates(const ResourceLoader::BatchResult& batch, IGraphicsContext& graphics,
                                               const Layout::Rect& viewport) {
    const bool stylesheet_ready = batch.is_ready(ResourceType::Stylesheet);
    const bool font_ready = batch.is_ready(ResourceType::Font);
    const bool image_ready = batch.is_ready(ResourceType::Image);
    if (stylesheet_ready && document_pipeline_->has_dom_tree()) {
        sync_extension_styles_before_stylesheet_update();
        handle_stylesheet_ready(graphics, viewport);
    }
    // A newly-arrived web font changes text metrics/rendering: re-apply styles so
    // the font resolver picks up the now-cached bytes, then relayout (FOUT).
    if (font_ready && !stylesheet_ready && document_pipeline_->has_dom_tree()) {
        (void)rebuild_document_and_sync_layout(graphics, viewport, "font_ready");
        mark_dirty("font_ready");
    }
    if (image_ready && document_pipeline_->has_render_tree()) {
        handle_image_ready(graphics, viewport);
    }
    // Checked on every batch (not just script arrivals) so a failed fetch —
    // which sets no ready flag — still unblocks deferred execution.
    if (scripts_pending_) {
        maybe_run_deferred_scripts(graphics, viewport);
    }
}

void Tab::maybe_run_deferred_scripts(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (!document_pipeline_->has_dom_tree()) return;
    if (!all_external_scripts_resolved()) return;
    scripts_pending_ = false;

    const auto scripts_start = Core::Clock::now();
    const bool mutated = run_document_scripts_now();
    HB_LOG_INFO("[perf] deferred run_scripts ms=" << Core::duration_ms(scripts_start, Core::Clock::now())
                                                  << " mutated=" << mutated);
    if (mutated) {
        (void)rebuild_document_and_sync_layout(graphics, viewport, "scripts_ready:mutation");
    }
    apply_load_mutations_after_document_ready(graphics, viewport);
    mark_dirty("scripts_ready");
}

bool Tab::all_external_scripts_resolved() const {
    for (const auto& src : document_pipeline_->external_script_srcs()) {
        auto resolved = ResourceRequestPlanning::resolve_request_url(navigation_lifecycle_.requested_url(), src);
        if (resolved.key.empty()) continue;
        const auto* entry = resource_loader_->find(resolved.key, ResourceType::Script);
        if (!entry) continue;  // never registered (rejected url) — nothing to wait for
        if (entry->state == ResourceState::Requested || entry->state == ResourceState::Loading) {
            return false;
        }
    }
    return true;
}

bool Tab::run_document_scripts_now() {
    // Point window.location at the document URL before any script reads it (7.2.5).
    document_pipeline_->set_location(navigation_lifecycle_.requested_url());
    return document_pipeline_->run_scripts([this](std::string_view src) -> std::optional<std::string_view> {
        auto resolved = ResourceRequestPlanning::resolve_request_url(navigation_lifecycle_.requested_url(), src);
        if (resolved.key.empty()) return std::nullopt;
        const auto* entry = resource_loader_->find(resolved.key, ResourceType::Script);
        if (!entry || entry->state != ResourceState::Ready) return std::nullopt;
        return std::string_view(entry->body);
    });
}

void Tab::sync_extension_styles_before_stylesheet_update() {
    if (!extension_css_dirty_) {
        return;
    }
    document_pipeline_->set_extension_style_blocks(extension_style_blocks_);
    extension_css_dirty_ = false;
}

void Tab::handle_document_ready(std::string_view document_url, std::string_view effective_url,
                                NetworkError document_error, IGraphicsContext& graphics, const Layout::Rect& viewport) {
    navigation_lifecycle_.update_from_document_ready(*resource_loader_, effective_url, document_error);
    auto document_body = resolve_document_ready_body(document_url);
    if (!document_body.has_value()) {
        return;
    }

    const auto rebuild_start = Core::Clock::now();
    HB_LOG_INFO("[pipeline] html size: " << document_body->size());
    if (!prepare_document_from_response(*document_body)) {
        return;
    }
    // Record this navigation so links pointing back at it style as :visited
    // (T-HIST-1). Both the requested and post-redirect URLs count, since an
    // anchor may resolve to either.
    document_pipeline_->mark_url_visited(navigation_lifecycle_.requested_url());
    document_pipeline_->mark_url_visited(effective_url);
    rebuild_after_document_ready(graphics, viewport);
    HB_LOG_INFO("[pipeline] render tree root children: " << document_pipeline_->render_tree_children());
    mark_dirty("document_ready");
    const auto rebuild_end = Core::Clock::now();
    HB_LOG_INFO("[perf] document rebuild ms=" << Core::duration_ms(rebuild_start, rebuild_end));
}

std::optional<std::string_view> Tab::resolve_document_ready_body(std::string_view document_url) const {
    const auto* entry = resource_loader_->find(document_url, ResourceType::Document);
    if (!entry) {
        return std::nullopt;
    }
    return entry->body;
}

void Tab::rebuild_after_document_ready(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    // Sub-timers bracket every step so a stall inside the rebuild span is
    // attributable from logs alone (the 21s DDG freeze hid between these).
    auto timed = [](const char* label, auto&& fn) {
        const auto start = Core::Clock::now();
        fn();
        HB_LOG_INFO("[perf] rebuild step " << label << " ms=" << Core::duration_ms(start, Core::Clock::now()));
    };

    TabDocumentReadyPolicy::log_discovered_resources(*document_pipeline_);
    timed("request_resources", [&] {
        TabDocumentReadyPolicy::request_discovered_resources(*resource_loader_, *document_pipeline_,
                                                             navigation_lifecycle_.requested_url());
    });
    bool has_render_tree = false;
    timed("build_and_layout", [&] {
        has_render_tree = rebuild_document_and_sync_layout(graphics, viewport, "handle_document_ready:initial_build");
    });
    if (has_render_tree) {
        timed("autofocus", [&] { apply_autofocus_after_rebuild(); });
    }
    // The load event fires only after every document-order script has run;
    // with external scripts still in flight it is dispatched from
    // maybe_run_deferred_scripts instead.
    if (!scripts_pending_) {
        timed("load_mutations", [&] { apply_load_mutations_after_document_ready(graphics, viewport); });
    }
}

void Tab::handle_stylesheet_ready(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    const auto style_update_start = Core::Clock::now();
    (void)rebuild_document_and_sync_layout(graphics, viewport, "handle_stylesheet_ready");
    const auto style_update_end = Core::Clock::now();
    HB_LOG_INFO("[perf] stylesheet update ms=" << Core::duration_ms(style_update_start, style_update_end));
    mark_dirty("stylesheet_ready");
}

void Tab::handle_image_ready(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    const auto image_update_start = Core::Clock::now();
    bool updated = document_pipeline_->update_image_resources(navigation_lifecycle_.requested_url());
    if (updated) {
        document_pipeline_->relayout(graphics, viewport);
        update_layout_state(viewport, "handle_image_ready:relayout");
    }
    const auto image_update_end = Core::Clock::now();
    HB_LOG_INFO("[perf] image update ms=" << Core::duration_ms(image_update_start, image_update_end)
                                          << " updated=" << updated);
    mark_dirty(updated ? "image_ready_updated" : "image_ready_noop");
}

bool Tab::prepare_document_from_response(std::string_view html) {
    if (!document_pipeline_->parse_html(html)) {
        return false;
    }
    navigation_lifecycle_.set_pending_commit_url();
    document_pipeline_->set_extension_style_blocks(extension_style_blocks_);
    extension_css_dirty_ = false;

    // External <script src> bodies must be fetched before any script runs so
    // inline and external scripts execute in document order (7.0.1). Asset- or
    // failed-synchronously resources resolve immediately; network fetches
    // arrive via the pending-update queue, so execution (and the load event,
    // see rebuild_after_document_ready) defers to maybe_run_deferred_scripts.
    const auto external_srcs = document_pipeline_->external_script_srcs();
    if (!external_srcs.empty()) {
        resource_loader_->request_scripts(external_srcs, navigation_lifecycle_.requested_url());
    }
    if (!all_external_scripts_resolved()) {
        scripts_pending_ = true;
        return true;
    }
    const auto scripts_start = Core::Clock::now();
    (void)run_document_scripts_now();
    HB_LOG_INFO("[perf] rebuild step run_scripts ms=" << Core::duration_ms(scripts_start, Core::Clock::now()));
    return true;
}

void Tab::apply_load_mutations_after_document_ready(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    auto load_result = document_pipeline_->dispatch_load();
    if (!load_result.mutated) {
        return;
    }

    if (rebuild_document_and_sync_layout(graphics, viewport, "handle_document_ready:load_mutation")) {
        apply_autofocus_after_rebuild();
    }
    mark_dirty();
}

void Tab::apply_autofocus_after_rebuild() {
    if (document_pipeline_->focus_autofocus_input()) {
        mark_dirty();
    }
}

void Tab::mark_dirty(std::string_view reason) {
    const bool was_dirty = dirty_;
    dirty_ = true;
    if (!reason.empty() && !was_dirty) {
        maybe_log_tab_dirty(reason, dirty_);
    }
}

bool Tab::rebuild_document_and_sync_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                           std::string_view reason) {
    bool has_render_tree =
        document_pipeline_->rebuild_and_layout(graphics, viewport, navigation_lifecycle_.requested_url());
    // Every rebuild requests the background images its computed styles now
    // reference: interaction restyles can switch a node to an image that was
    // not discoverable at load time (DDG's loupe swaps white<->gray with
    // :focus, and autofocus means only one variant exists at load). The
    // resource store dedupes, so re-requesting known URLs is a no-op.
    resource_loader_->request_images(document_pipeline_->background_image_links(),
                                     navigation_lifecycle_.requested_url());
    // @font-face web fonts the just-applied styles reference; the store dedupes,
    // so re-requesting known urls each rebuild is a no-op (T-FONT-FACE-1).
    resource_loader_->request_fonts(document_pipeline_->font_requests(), navigation_lifecycle_.requested_url());
    if (has_render_tree) {
        update_layout_state(viewport, reason);
    }
    return has_render_tree;
}

void Tab::begin_navigation_session(std::string_view url) {
    navigation_lifecycle_.begin_navigation_from_input(url);
    reset_document_state();
}

void Tab::reset_document_state() {
    document_pipeline_->reset();
    resource_loader_->reset();
    layout_state_.reset();
    navigation_lifecycle_.clear_pending_commit_url();
    animation_ticker_.reset();
    timer_has_tick_ = false;
    timer_clock_ms_ = 0.0;
    scripts_pending_ = false;
    mark_dirty();
}

void Tab::update_layout_state(const Layout::Rect& viewport, std::string_view reason) {
    float old_content_height = layout_state_.content_height;
    float old_scroll_y = layout_state_.scroll_y;
    layout_state_.update(viewport, document_pipeline_->content_height());
    mark_dirty();
    if (layout_state_debug_enabled()) {
        const float max_scroll = std::max(0.0f, layout_state_.content_height - viewport.height);
        HB_LOG_WARN("[layout-debug] reason="
                    << reason << " viewport_h=" << viewport.height << " content_h_old=" << old_content_height
                    << " content_h_new=" << layout_state_.content_height << " scroll_old=" << old_scroll_y
                    << " scroll_new=" << layout_state_.scroll_y << " max_scroll=" << max_scroll);
    }
}

bool Tab::allow_insecure_for_current_host() {
    return navigation_lifecycle_.allow_insecure_for_current_host(*resource_loader_);
}

}  // namespace Hummingbird::Engine
