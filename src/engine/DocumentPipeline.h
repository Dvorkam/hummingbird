#pragma once

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/InputEvent.h"
#include "layout/Geometry.h"
#include "layout/TreeBuilder.h"
#include "renderer/Painter.h"
#include "style/StyleEngine.h"

namespace Hummingbird {
class IResourceProvider;
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::DOM {
class Element;
class Node;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class ResourceStore;

class DocumentPipeline {
public:
    struct PaintContext {
        Layout::Rect viewport;
        bool debug_outlines = false;
        float scroll_y = 0.0f;
    };

    struct HitTestContext {
        Layout::Point point;
        Layout::Rect viewport;
        std::string_view base_url;
        float scroll_y = 0.0f;
    };

    struct InputEditResult {
        bool handled = false;
        bool needs_repaint = false;
    };

    DocumentPipeline(ResourceStore* resource_store, IResourceProvider* resource_provider);
    ~DocumentPipeline();

    DocumentPipeline(const DocumentPipeline&) = delete;
    DocumentPipeline& operator=(const DocumentPipeline&) = delete;
    DocumentPipeline(DocumentPipeline&&) = delete;
    DocumentPipeline& operator=(DocumentPipeline&&) = delete;

    void reset();

    bool parse_html(std::string_view html);
    void apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport, std::string_view base_url);
    bool update_image_resources(std::string_view base_url);
    void relayout(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void paint(IGraphicsContext& graphics, const PaintContext& context);
    std::optional<std::string> hit_test_link(const HitTestContext& context) const;
    bool focus_input_at(const HitTestContext& context);
    bool clear_input_focus();
    bool has_focused_input() const { return focused_input_ != nullptr; }
    InputEditResult handle_text_input(std::string_view text);
    InputEditResult handle_key_down(const InputEvent& event);
    std::optional<std::string> focused_input_value() const;

    bool has_dom_tree() const { return static_cast<bool>(dom_tree_); }
    bool has_render_tree() const { return static_cast<bool>(render_tree_); }
    float content_height() const { return content_height_; }
    size_t render_tree_children() const;
    const std::vector<std::string>& stylesheet_links() const { return stylesheet_links_; }
    const std::vector<std::string>& image_links() const { return image_links_; }

private:
    static constexpr size_t kDomArenaBlockSize = 2 * 1024 * 1024;
    static constexpr size_t kDomArenaMaxBlocks = 16;

    std::string build_css_source(std::string_view base_url) const;
    void parse_and_apply_css(const std::string& css);
    bool build_render_tree();
    DOM::Element* hit_test_input(const HitTestContext& context) const;
    void paint_input_controls(IGraphicsContext& graphics, const PaintContext& context) const;

    ResourceStore* resource_store_ = nullptr;
    IResourceProvider* resource_provider_ = nullptr;

    Css::StyleEngine style_engine_;
    Layout::TreeBuilder tree_builder_;
    Renderer::Painter painter_;

    Core::ArenaAllocator dom_arena_{kDomArenaBlockSize, kDomArenaMaxBlocks};
    Core::ArenaPtr<DOM::Node> dom_tree_;
    std::unique_ptr<Layout::RenderObject> render_tree_;

    std::vector<std::string> style_blocks_;
    std::vector<std::string> stylesheet_links_;
    std::vector<std::string> image_links_;

    float content_height_ = 0.0f;
    DOM::Element* focused_input_ = nullptr;
    std::string::size_type input_caret_ = 0;
};

}  // namespace Hummingbird::Engine
