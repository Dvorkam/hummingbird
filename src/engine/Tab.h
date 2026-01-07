#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/INetwork.h"
#include "core/platform_api/IResourceProvider.h"
#include "engine/ResourceStore.h"
#include "layout/Geometry.h"
#include "layout/TreeBuilder.h"
#include "renderer/Painter.h"
#include "style/StyleEngine.h"

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Engine {

class Tab {
public:
    Tab(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
        ImageDecoderPtr image_decoder);
    ~Tab();

    Tab(const Tab&) = delete;
    Tab& operator=(const Tab&) = delete;
    Tab(Tab&&) = delete;
    Tab& operator=(Tab&&) = delete;

    void shutdown();

    void navigate(std::string_view url);

    // Processes pending navigation results and keeps layout in sync with the viewport.
    // Returns true if the document changed in a way that needs repainting.
    bool tick(IGraphicsContext& graphics, const Layout::Rect& viewport);

    // Paints the current document into the given viewport using the current scroll offset.
    void paint(IGraphicsContext& graphics, const Layout::Rect& viewport, bool debug_outlines);

    void scroll_by(float delta_px, float viewport_height);

    float scroll_y() const { return scroll_y_; }
    float content_height() const { return content_height_; }
    std::string_view requested_url() const { return requested_url_; }
    std::optional<ResourceView> resource_view(std::string_view url, ResourceType type) const;

private:
    struct PendingResourceUpdate {
        ResourceType type;
        std::string url;
        std::string body;
        bool success;
    };

    void clamp_scroll(float viewport_height);

    void consume_pending_resources(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void enqueue_resource_update(ResourceType type, std::string url, std::string body, bool success);
    std::vector<PendingResourceUpdate> take_pending_resources();

    void rebuild_from_html(IGraphicsContext& graphics, const Layout::Rect& viewport, const std::string& html);
    void reset_document_state();

    bool parse_html(const std::string& html, std::vector<std::string>& style_blocks,
                    std::vector<std::string>& stylesheet_links, std::vector<std::string>& image_links);
    void request_stylesheets(const std::vector<std::string>& stylesheet_links);
    void request_images(const std::vector<std::string>& image_links);
    void apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport);
    std::string build_css_source(const std::vector<std::string>& style_blocks,
                                 const std::vector<std::string>& stylesheet_links) const;
    void parse_and_apply_css(const std::string& css);
    bool build_render_tree();
    bool update_image_resources();
    void relayout(IGraphicsContext& graphics, const Layout::Rect& viewport);

private:
    std::atomic<bool> shutting_down_{false};

    // Async HTML handoff (network thread -> main thread)
    std::mutex pending_mutex_;
    std::vector<PendingResourceUpdate> pending_resources_;

    // Deps / subsystems
    NetworkPtr network_;
    NetworkPtr fallback_network_;
    ResourceProviderPtr resource_provider_;
    ImageDecoderPtr image_decoder_;

    Css::StyleEngine style_engine_;
    Layout::TreeBuilder tree_builder_;
    Renderer::Painter painter_;
    ResourceStore resource_store_;

    // Navigation race protection
    std::atomic<uint64_t> nav_counter_{0};
    std::atomic<uint64_t> active_nav_{0};

    // Document / layout state
    ArenaAllocator dom_arena_{2 * 1024 * 1024};
    ArenaPtr<DOM::Node> dom_tree_;
    std::unique_ptr<Layout::RenderObject> render_tree_;

    std::string requested_url_;
    std::vector<std::string> style_blocks_;
    std::vector<std::string> stylesheet_links_;
    std::vector<std::string> image_links_;
    float scroll_y_ = 0.0f;
    float content_height_ = 0.0f;
    Layout::Rect last_viewport_{0, 0, 0, 0};
    bool has_viewport_ = false;

    bool dirty_ = true;
};

}  // namespace Hummingbird::Engine
