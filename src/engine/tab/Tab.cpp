#include "engine/tab/Tab.h"

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <utility>
#include <vector>

#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "core/utils/Url.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Engine {

namespace {
SecurityState security_state_for_url(std::string_view url) {
    auto parsed = Core::parse_absolute_url(url);
    if (!parsed) return SecurityState::Unknown;
    if (parsed->scheme == "https") return SecurityState::Secure;
    if (parsed->scheme == "http") return SecurityState::InsecureHttp;
    return SecurityState::Unknown;
}
}  // namespace

Tab::Tab(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
         ImageDecoderPtr image_decoder, ScriptEnginePtr script_engine)
    : resource_loader_(std::move(network), std::move(fallback_network), std::move(resource_provider),
                       std::move(image_decoder)),
      document_pipeline_(&resource_loader_.store(), resource_loader_.resource_provider(),
                         resource_loader_.image_decoder(), std::move(script_engine)) {}

Tab::~Tab() {
    shutdown();
}

void Tab::shutdown() {
    if (shutting_down_.exchange(true, std::memory_order_relaxed)) return;
    resource_loader_.shutdown();
}

void Tab::navigate(std::string_view url) {
    if (shutting_down_.load(std::memory_order_relaxed)) return;

    std::string normalized = Core::normalize_input_url(url);
    requested_url_ = std::move(normalized);
    pending_navigation_commit_url_.reset();
    security_state_ = security_state_for_url(requested_url_);
    reset_document_state();
    resource_loader_.navigate(requested_url_);
}

void Tab::navigate(const FormSubmission& submission) {
    if (shutting_down_.load(std::memory_order_relaxed)) return;

    std::string normalized = Core::normalize_input_url(submission.url);
    requested_url_ = std::move(normalized);
    pending_navigation_commit_url_.reset();
    security_state_ = security_state_for_url(requested_url_);
    reset_document_state();

    ResourceLoader::DocumentRequest request{};
    if (submission.method == FormSubmitMethod::Post) {
        request.method = ResourceLoader::DocumentRequest::Method::Post;
        request.body = submission.body;
        request.content_type = submission.content_type;
    }
    resource_loader_.navigate(requested_url_, request);
}

bool Tab::tick(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (shutting_down_.load(std::memory_order_relaxed)) return false;

    consume_pending_resources(graphics, viewport);

    if (extension_css_dirty_ && document_pipeline_.has_dom_tree()) {
        document_pipeline_.set_extension_style_blocks(extension_style_blocks_);
        bool has_render_tree = document_pipeline_.rebuild_and_layout(graphics, viewport, requested_url_);
        resource_loader_.request_images(document_pipeline_.background_image_links(), requested_url_);
        if (has_render_tree) {
            update_layout_state(viewport);
        }
        extension_css_dirty_ = false;
        dirty_ = true;
    }

    if (document_pipeline_.has_render_tree()) {
        if (layout_state_.viewport_changed(viewport)) {
            document_pipeline_.relayout(graphics, viewport);
            update_layout_state(viewport);
        }
    }

    const auto now = Core::Clock::now();
    if (!has_animation_tick_) {
        last_animation_tick_ = now;
        has_animation_tick_ = true;
    } else {
        int delta_ms = static_cast<int>(Core::duration_ms(last_animation_tick_, now));
        last_animation_tick_ = now;
        if (resource_loader_.store().tick_animations(delta_ms) && document_pipeline_.has_render_tree()) {
            if (document_pipeline_.update_image_resources(requested_url_)) {
                dirty_ = true;
            }
        }
    }

    bool dirty = dirty_;
    dirty_ = false;
    return dirty;
}

void Tab::paint(IGraphicsContext& graphics, const Layout::Rect& viewport, bool debug_outlines) {
    if (!document_pipeline_.has_render_tree()) return;
    graphics.set_text_cache_owner(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(this)));
    DocumentPipeline::PaintContext context{viewport, debug_outlines, layout_state_.scroll_y};
    document_pipeline_.paint(graphics, context);
    graphics.set_text_cache_owner(0);
}

void Tab::paint_controls(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (!document_pipeline_.has_render_tree()) return;
    graphics.set_text_cache_owner(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(this)));
    DocumentPipeline::PaintContext context{viewport, false, layout_state_.scroll_y};
    document_pipeline_.paint_controls(graphics, context, true);
    graphics.set_text_cache_owner(0);
}

std::optional<std::string> Tab::hit_test_link(const Layout::Point& point, const Layout::Rect& viewport) const {
    DocumentPipeline::HitTestContext context{point, viewport, requested_url_, layout_state_.scroll_y};
    return document_pipeline_.hit_test_link(context);
}

Tab::ClickResult Tab::dispatch_click(const Layout::Point& point, const Layout::Rect& viewport,
                                     IGraphicsContext& graphics) {
    DocumentPipeline::HitTestContext context{point, viewport, requested_url_, layout_state_.scroll_y};
    auto result = document_pipeline_.dispatch_click(context);
    if (result.mutated) {
        if (document_pipeline_.rebuild_and_layout(graphics, viewport, requested_url_)) {
            update_layout_state(viewport);
        }
        dirty_ = true;
    }
    return {result.handled, result.mutated};
}

std::optional<FormSubmission> Tab::submit_form_at(const Layout::Point& point, const Layout::Rect& viewport) const {
    DocumentPipeline::HitTestContext context{point, viewport, requested_url_, layout_state_.scroll_y};
    return document_pipeline_.submit_form_at(context);
}

bool Tab::focus_input_at(const Layout::Point& point, const Layout::Rect& viewport) {
    DocumentPipeline::HitTestContext context{point, viewport, requested_url_, layout_state_.scroll_y};
    return document_pipeline_.focus_input_at(context);
}

bool Tab::clear_input_focus() {
    return document_pipeline_.clear_input_focus();
}

bool Tab::has_focused_input() const {
    return document_pipeline_.has_focused_input();
}

bool Tab::handle_text_input(std::string_view text) {
    auto result = document_pipeline_.handle_text_input(text);
    return result.handled;
}

Tab::KeyResult Tab::handle_key_down(const InputEvent& event) {
    auto result = document_pipeline_.handle_key_down(event, requested_url_);
    return {result.handled, result.needs_repaint, std::move(result.submitted_form)};
}

std::optional<std::string> Tab::focused_input_value() const {
    return document_pipeline_.focused_input_value();
}

std::optional<std::string> Tab::consume_navigation_commit_url() {
    if (!pending_navigation_commit_url_) {
        return std::nullopt;
    }
    auto url = std::move(pending_navigation_commit_url_);
    pending_navigation_commit_url_.reset();
    return url;
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
    dirty_ = true;
    return true;
}

std::optional<ResourceView> Tab::resource_view(std::string_view url, ResourceType type) const {
    return resource_loader_.view(url, type);
}

void Tab::scroll_by(float delta_px, float viewport_height) {
    layout_state_.scroll_by(delta_px, viewport_height);
    dirty_ = true;
}

void Tab::consume_pending_resources(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    auto result = resource_loader_.consume_pending_updates();
    if (result.pending_count == 0) return;

    if (result.document_ready) {
        handle_document_ready(result, graphics, viewport);
        return;
    }
    if (result.stylesheet_ready && document_pipeline_.has_dom_tree()) {
        if (extension_css_dirty_) {
            document_pipeline_.set_extension_style_blocks(extension_style_blocks_);
            extension_css_dirty_ = false;
        }
        handle_stylesheet_ready(graphics, viewport);
    }
    if (result.image_ready && document_pipeline_.has_render_tree()) {
        handle_image_ready(graphics, viewport);
    }
}

void Tab::handle_document_ready(const ResourceLoader::BatchResult& result, IGraphicsContext& graphics,
                                const Layout::Rect& viewport) {
    if (!result.effective_url.empty()) {
        requested_url_ = result.effective_url;
    }
    if (result.document_error == NetworkError::TlsVerificationFailed) {
        security_state_ = SecurityState::InsecureTls;
    } else if (resource_loader_.is_insecure_allowed_for_url(requested_url_)) {
        security_state_ = SecurityState::InsecureTls;
    } else {
        security_state_ = security_state_for_url(requested_url_);
    }
    const auto* entry = resource_loader_.find(result.document_url, ResourceType::Document);
    if (!entry) {
        return;
    }

    const auto rebuild_start = Core::Clock::now();
    HB_LOG_INFO("[pipeline] html size: " << entry->body.size());
    if (!document_pipeline_.parse_html(entry->body)) {
        return;
    }
    pending_navigation_commit_url_ = requested_url_;
    document_pipeline_.set_extension_style_blocks(extension_style_blocks_);
    extension_css_dirty_ = false;
    document_pipeline_.run_scripts();
    if (!document_pipeline_.stylesheet_links().empty()) {
        HB_LOG_INFO("[pipeline] discovered stylesheet links: " << document_pipeline_.stylesheet_links().size());
    }
    if (!document_pipeline_.image_links().empty()) {
        HB_LOG_INFO("[pipeline] discovered image sources: " << document_pipeline_.image_links().size());
    }

    resource_loader_.request_stylesheets(document_pipeline_.stylesheet_links(), requested_url_);
    resource_loader_.request_images(document_pipeline_.image_links(), requested_url_);
    bool has_render_tree = document_pipeline_.rebuild_and_layout(graphics, viewport, requested_url_);
    if (!document_pipeline_.background_image_links().empty()) {
        HB_LOG_INFO("[pipeline] discovered background images: " << document_pipeline_.background_image_links().size());
    }
    resource_loader_.request_images(document_pipeline_.background_image_links(), requested_url_);
    if (has_render_tree) {
        update_layout_state(viewport);
        if (document_pipeline_.focus_autofocus_input()) {
            dirty_ = true;
        }
    }
    auto load_result = document_pipeline_.dispatch_load();
    if (load_result.mutated) {
        if (document_pipeline_.rebuild_and_layout(graphics, viewport, requested_url_)) {
            update_layout_state(viewport);
            if (document_pipeline_.focus_autofocus_input()) {
                dirty_ = true;
            }
        }
        dirty_ = true;
    }
    HB_LOG_INFO("[pipeline] render tree root children: " << document_pipeline_.render_tree_children());
    dirty_ = true;
    const auto rebuild_end = Core::Clock::now();
    HB_LOG_INFO("[perf] document rebuild ms=" << Core::duration_ms(rebuild_start, rebuild_end));
}

void Tab::handle_stylesheet_ready(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    const auto style_update_start = Core::Clock::now();
    bool has_render_tree = document_pipeline_.rebuild_and_layout(graphics, viewport, requested_url_);
    resource_loader_.request_images(document_pipeline_.background_image_links(), requested_url_);
    if (has_render_tree) {
        update_layout_state(viewport);
    }
    const auto style_update_end = Core::Clock::now();
    HB_LOG_INFO("[perf] stylesheet update ms=" << Core::duration_ms(style_update_start, style_update_end));
    dirty_ = true;
}

void Tab::handle_image_ready(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    const auto image_update_start = Core::Clock::now();
    bool updated = document_pipeline_.update_image_resources(requested_url_);
    if (updated) {
        document_pipeline_.relayout(graphics, viewport);
        update_layout_state(viewport);
    }
    const auto image_update_end = Core::Clock::now();
    HB_LOG_INFO("[perf] image update ms=" << Core::duration_ms(image_update_start, image_update_end)
                                          << " updated=" << updated);
    dirty_ = true;
}

void Tab::reset_document_state() {
    document_pipeline_.reset();
    resource_loader_.reset();
    layout_state_.reset();
    pending_navigation_commit_url_.reset();
    has_animation_tick_ = false;
    dirty_ = true;
}

void Tab::update_layout_state(const Layout::Rect& viewport) {
    layout_state_.update(viewport, document_pipeline_.content_height());
    dirty_ = true;
}

bool Tab::allow_insecure_for_current_host() {
    auto parsed = Core::parse_absolute_url(requested_url_);
    if (!parsed || parsed->host.empty()) {
        return false;
    }
    resource_loader_.allow_insecure_host(parsed->host);
    security_state_ = SecurityState::InsecureTls;
    return true;
}

bool Tab::LayoutState::viewport_changed(const Layout::Rect& viewport) const {
    return !has_viewport || viewport.x != last_viewport.x || viewport.y != last_viewport.y ||
           viewport.width != last_viewport.width || viewport.height != last_viewport.height;
}

void Tab::LayoutState::reset() {
    scroll_y = 0.0f;
    content_height = 0.0f;
    has_viewport = false;
    last_viewport = Layout::Rect{0, 0, 0, 0};
}

void Tab::LayoutState::update(const Layout::Rect& viewport, float new_content_height) {
    content_height = new_content_height;
    last_viewport = viewport;
    has_viewport = true;
    clamp_scroll(viewport.height);
}

void Tab::LayoutState::clamp_scroll(float viewport_height) {
    const float max_scroll = std::max(0.0f, content_height - viewport_height);
    scroll_y = std::clamp(scroll_y, 0.0f, max_scroll);
}

void Tab::LayoutState::scroll_by(float delta_px, float viewport_height) {
    scroll_y -= delta_px;
    clamp_scroll(viewport_height);
}

}  // namespace Hummingbird::Engine
