#pragma once

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/forms/FormSubmission.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IResourceProvider;
class IGraphicsContext;
class IImageDecoder;
class IScriptEngine;
struct InputEvent;
}  // namespace Hummingbird

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
    };

    struct PaintContext {
        Layout::Rect viewport;
        bool debug_outlines = false;
        float scroll_y = 0.0f;
    };

    struct InputEditResult {
        bool handled = false;
        bool needs_repaint = false;
        std::optional<FormSubmission> submitted_form;
    };

    struct ScriptDispatchResult {
        bool handled = false;
        bool mutated = false;
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
    bool run_scripts();
    void set_extension_style_blocks(const std::vector<std::string>& style_blocks);
    void apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport, std::string_view base_url);
    bool rebuild_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport, std::string_view base_url);
    bool update_image_resources(std::string_view base_url);
    // True when the new viewport flips any @media rule vs. the last style
    // application — the caller must restyle, a plain relayout is not enough.
    bool needs_restyle_for_viewport(const Layout::Rect& viewport) const;
    void relayout(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void paint(IGraphicsContext& graphics, const PaintContext& context);
    void paint_controls(IGraphicsContext& graphics, const PaintContext& context, bool repaint_background);
    bool has_dom_tree() const;
    bool has_render_tree() const;
    float content_height() const;
    size_t render_tree_children() const;
    const std::vector<std::string>& stylesheet_links() const;
    const std::vector<std::string>& image_links() const;
    const std::vector<std::string>& background_image_links() const;

    // --- interaction ---
    ScriptDispatchResult dispatch_click(const HitTestContext& context);
    ScriptDispatchResult dispatch_load();
    std::optional<std::string> hit_test_link(const HitTestContext& context) const;
    std::optional<FormSubmission> submit_form_at(const HitTestContext& context) const;
    bool focus_input_at(const HitTestContext& context);
    bool focus_autofocus_input();
    bool clear_input_focus();
    bool set_control_interaction_at(const HitTestContext& context);
    bool clear_control_interaction();
    bool has_focused_input() const;
    InputEditResult handle_text_input(std::string_view text);
    InputEditResult handle_key_down(const InputEvent& event, std::string_view base_url);
    std::optional<std::string> focused_input_value() const;

private:
    std::unique_ptr<DocumentResources> resources_;
    std::unique_ptr<DocumentModel> model_;
    std::unique_ptr<DocumentInteraction> interaction_;
    std::unique_ptr<DocumentRenderer> renderer_;
    std::unique_ptr<DocumentStyleCoordinator> style_coordinator_;
    std::unique_ptr<DocumentScripting> scripting_;
};

}  // namespace Hummingbird::Engine
