#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/SecurityState.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/INetwork.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/platform_api/InputEvent.h"
#include "engine/DocumentPipeline.h"
#include "engine/ResourceLoader.h"
#include "engine/ResourceStore.h"
#include "layout/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Engine {
class Tab {
public:
    struct KeyResult {
        bool handled = false;
        bool needs_repaint = false;
        std::optional<std::string> submitted_url;
    };
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

    // Returns a resolved link URL for the render node under the window-space point.
    std::optional<std::string> hit_test_link(const Layout::Point& point, const Layout::Rect& viewport) const;
    bool focus_input_at(const Layout::Point& point, const Layout::Rect& viewport);
    bool clear_input_focus();
    bool has_focused_input() const;
    bool handle_text_input(std::string_view text);
    KeyResult handle_key_down(const InputEvent& event);
    std::optional<std::string> focused_input_value() const;

    void scroll_by(float delta_px, float viewport_height);

    float scroll_y() const { return scroll_y_; }
    float content_height() const { return content_height_; }
    std::string_view requested_url() const { return requested_url_; }
    SecurityState security_state() const { return security_state_; }
    std::optional<ResourceView> resource_view(std::string_view url, ResourceType type) const;

    bool allow_insecure_for_current_host();

private:
    void clamp_scroll(float viewport_height);

    void consume_pending_resources(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void handle_document_ready(const ResourceLoader::BatchResult& result, IGraphicsContext& graphics,
                               const Layout::Rect& viewport);
    void handle_stylesheet_ready(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void handle_image_ready(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void reset_document_state();
    void update_layout_state(const Layout::Rect& viewport);

private:
    std::atomic<bool> shutting_down_{false};

    ResourceLoader resource_loader_;
    DocumentPipeline document_pipeline_;

    std::string requested_url_;
    SecurityState security_state_ = SecurityState::Unknown;
    float scroll_y_ = 0.0f;
    float content_height_ = 0.0f;
    Layout::Rect last_viewport_{0, 0, 0, 0};
    bool has_viewport_ = false;

    bool dirty_ = true;
};

}  // namespace Hummingbird::Engine
