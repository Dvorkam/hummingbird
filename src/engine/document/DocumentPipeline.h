#pragma once

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IScriptEngine.h"
#include "core/platform_api/InputEvent.h"
#include "engine/document/DocumentInputController.h"
#include "engine/document/DocumentModel.h"
#include "engine/document/DocumentPainter.h"
#include "engine/document/DocumentResources.h"
#include "engine/forms/FormSubmission.h"
#include "engine/script/DocumentScriptController.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IResourceProvider;
class IGraphicsContext;
class IImageDecoder;
}  // namespace Hummingbird

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
        std::optional<FormSubmission> submitted_form;
    };

    using ScriptDispatchResult = DocumentScriptController::ScriptDispatchResult;

    DocumentPipeline(ResourceStore* resource_store, IResourceProvider* resource_provider, IImageDecoder* image_decoder,
                     ScriptEnginePtr script_engine);
    ~DocumentPipeline();

    DocumentPipeline(const DocumentPipeline&) = delete;
    DocumentPipeline& operator=(const DocumentPipeline&) = delete;
    DocumentPipeline(DocumentPipeline&&) = delete;
    DocumentPipeline& operator=(DocumentPipeline&&) = delete;

    void reset();

    bool parse_html(std::string_view html);
    bool run_scripts();
    void set_extension_style_blocks(const std::vector<std::string>& style_blocks);
    ScriptDispatchResult dispatch_click(const HitTestContext& context);
    ScriptDispatchResult dispatch_load();
    void apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport, std::string_view base_url);
    bool rebuild_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport, std::string_view base_url);
    bool update_image_resources(std::string_view base_url);
    void relayout(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void paint(IGraphicsContext& graphics, const PaintContext& context);
    void paint_controls(IGraphicsContext& graphics, const PaintContext& context, bool repaint_background);
    std::optional<std::string> hit_test_link(const HitTestContext& context) const;
    std::optional<FormSubmission> submit_form_at(const HitTestContext& context) const;
    bool focus_input_at(const HitTestContext& context);
    bool focus_autofocus_input();
    bool clear_input_focus();
    bool has_focused_input() const { return input_controller_.has_focus(); }
    InputEditResult handle_text_input(std::string_view text);
    InputEditResult handle_key_down(const InputEvent& event, std::string_view base_url);
    std::optional<std::string> focused_input_value() const;

    bool has_dom_tree() const { return model_.has_dom_tree(); }
    bool has_render_tree() const { return model_.has_render_tree(); }
    float content_height() const { return content_height_; }
    size_t render_tree_children() const;
    const std::vector<std::string>& stylesheet_links() const { return model_.stylesheet_links(); }
    const std::vector<std::string>& image_links() const { return model_.image_links(); }
    const std::vector<std::string>& background_image_links() const { return model_.background_image_links(); }

private:
    float content_height_ = 0.0f;
    DocumentInputController input_controller_;
    DocumentResources resources_;
    DocumentModel model_;
    DocumentPainter painter_;
    DocumentScriptController script_controller_;
    std::vector<std::string> extension_style_blocks_;
};

}  // namespace Hummingbird::Engine
