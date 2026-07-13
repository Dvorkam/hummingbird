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
#include "engine/document/DocumentPipeline.h"
#include "engine/resources/ResourceLoader.h"
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
         std::unique_ptr<IScriptEngine> script_engine)
    : resource_loader_(std::make_unique<ResourceLoader>(std::move(network), std::move(fallback_network),
                                                        std::move(resource_provider), std::move(image_decoder))),
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

void Tab::navigate(std::string_view url) {
    if (shutting_down_.load(std::memory_order_relaxed)) return;

    begin_navigation_session(url);
    resource_loader_->navigate(navigation_lifecycle_.requested_url());
}

void Tab::navigate(const FormSubmission& submission) {
    if (shutting_down_.load(std::memory_order_relaxed)) return;

    begin_navigation_session(submission.url);

    ResourceLoader::DocumentRequest request{};
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

    bool dirty = dirty_;
    dirty_ = false;
    return dirty;
}

void Tab::apply_extension_css_if_needed(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (extension_css_dirty_ && document_pipeline_->has_dom_tree()) {
        document_pipeline_->set_extension_style_blocks(extension_style_blocks_);
        (void)rebuild_document_and_sync_layout(graphics, viewport, "tick:extension_css", true);
        extension_css_dirty_ = false;
        mark_dirty("extension_css");
    }
}

void Tab::relayout_if_viewport_changed(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (document_pipeline_->has_render_tree()) {
        if (layout_state_.viewport_changed(viewport)) {
            document_pipeline_->relayout(graphics, viewport);
            update_layout_state(viewport, "tick:viewport_changed");
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

Tab::ClickResult Tab::dispatch_click(const Layout::Point& point, const Layout::Rect& viewport,
                                     IGraphicsContext& graphics) {
    auto result = document_pipeline_->dispatch_click(
        make_hit_test_context(point, viewport, navigation_lifecycle_.requested_url(), layout_state_.scroll_y));
    if (result.mutated) {
        (void)rebuild_document_and_sync_layout(graphics, viewport, "dispatch_click:script_mutation", false);
        mark_dirty();
    }
    return {result.handled, result.mutated};
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
    return rebuild_document_and_sync_layout(graphics, viewport, "refresh_styles_for_interaction", false);
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
    return {result.handled, result.needs_repaint, std::move(result.submitted_form)};
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

    if (result.document_ready) {
        handle_document_ready(result.document_url, result.effective_url, result.document_error, graphics, viewport);
        return;
    }
    process_incremental_resource_updates(result.stylesheet_ready, result.image_ready, graphics, viewport);
}

void Tab::process_incremental_resource_updates(bool stylesheet_ready, bool image_ready, IGraphicsContext& graphics,
                                               const Layout::Rect& viewport) {
    if (stylesheet_ready && document_pipeline_->has_dom_tree()) {
        sync_extension_styles_before_stylesheet_update();
        handle_stylesheet_ready(graphics, viewport);
    }
    if (image_ready && document_pipeline_->has_render_tree()) {
        handle_image_ready(graphics, viewport);
    }
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
        has_render_tree =
            rebuild_document_and_sync_layout(graphics, viewport, "handle_document_ready:initial_build", true);
    });
    if (has_render_tree) {
        timed("autofocus", [&] { apply_autofocus_after_rebuild(); });
    }
    timed("load_mutations", [&] { apply_load_mutations_after_document_ready(graphics, viewport); });
}

void Tab::handle_stylesheet_ready(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    const auto style_update_start = Core::Clock::now();
    (void)rebuild_document_and_sync_layout(graphics, viewport, "handle_stylesheet_ready", true);
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
    const auto scripts_start = Core::Clock::now();
    document_pipeline_->run_scripts();
    HB_LOG_INFO("[perf] rebuild step run_scripts ms=" << Core::duration_ms(scripts_start, Core::Clock::now()));
    return true;
}

void Tab::apply_load_mutations_after_document_ready(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    auto load_result = document_pipeline_->dispatch_load();
    if (!load_result.mutated) {
        return;
    }

    if (rebuild_document_and_sync_layout(graphics, viewport, "handle_document_ready:load_mutation", true)) {
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
                                           std::string_view reason, bool request_background_images) {
    bool has_render_tree =
        document_pipeline_->rebuild_and_layout(graphics, viewport, navigation_lifecycle_.requested_url());
    if (request_background_images) {
        resource_loader_->request_images(document_pipeline_->background_image_links(),
                                         navigation_lifecycle_.requested_url());
    }
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
