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

    class LayoutApi {
    public:
        bool parse_html(std::string_view html) { return pipeline_.parse_html(html); }
        bool run_scripts() { return pipeline_.run_scripts(); }
        void set_extension_style_blocks(const std::vector<std::string>& style_blocks) {
            pipeline_.set_extension_style_blocks(style_blocks);
        }
        void apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                     std::string_view base_url) {
            pipeline_.apply_styles_and_layout(graphics, viewport, base_url);
        }
        bool rebuild_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport, std::string_view base_url) {
            return pipeline_.rebuild_and_layout(graphics, viewport, base_url);
        }
        bool update_image_resources(std::string_view base_url) { return pipeline_.update_image_resources(base_url); }
        void relayout(IGraphicsContext& graphics, const Layout::Rect& viewport) {
            pipeline_.relayout(graphics, viewport);
        }
        void paint(IGraphicsContext& graphics, const PaintContext& context) { pipeline_.paint(graphics, context); }
        void paint_controls(IGraphicsContext& graphics, const PaintContext& context, bool repaint_background) {
            pipeline_.paint_controls(graphics, context, repaint_background);
        }
        bool has_dom_tree() const { return pipeline_.has_dom_tree(); }
        bool has_render_tree() const { return pipeline_.has_render_tree(); }
        float content_height() const { return pipeline_.content_height(); }
        size_t render_tree_children() const { return pipeline_.render_tree_children(); }
        const std::vector<std::string>& stylesheet_links() const { return pipeline_.stylesheet_links(); }
        const std::vector<std::string>& image_links() const { return pipeline_.image_links(); }
        const std::vector<std::string>& background_image_links() const { return pipeline_.background_image_links(); }

    private:
        friend class DocumentPipeline;
        explicit LayoutApi(DocumentPipeline& pipeline) : pipeline_(pipeline) {}

        DocumentPipeline& pipeline_;
    };

    class InteractionApi {
    public:
        ScriptDispatchResult dispatch_click(const HitTestContext& context) { return pipeline_.dispatch_click(context); }
        ScriptDispatchResult dispatch_load() { return pipeline_.dispatch_load(); }
        std::optional<std::string> hit_test_link(const HitTestContext& context) const {
            return pipeline_.hit_test_link(context);
        }
        std::optional<FormSubmission> submit_form_at(const HitTestContext& context) const {
            return pipeline_.submit_form_at(context);
        }
        bool focus_input_at(const HitTestContext& context) { return pipeline_.focus_input_at(context); }
        bool focus_autofocus_input() { return pipeline_.focus_autofocus_input(); }
        bool clear_input_focus() { return pipeline_.clear_input_focus(); }
        bool set_control_interaction_at(const HitTestContext& context) {
            return pipeline_.set_control_interaction_at(context);
        }
        bool clear_control_interaction() { return pipeline_.clear_control_interaction(); }
        bool has_focused_input() const { return pipeline_.has_focused_input(); }
        InputEditResult handle_text_input(std::string_view text) { return pipeline_.handle_text_input(text); }
        InputEditResult handle_key_down(const InputEvent& event, std::string_view base_url) {
            return pipeline_.handle_key_down(event, base_url);
        }
        std::optional<std::string> focused_input_value() const { return pipeline_.focused_input_value(); }

    private:
        friend class DocumentPipeline;
        explicit InteractionApi(DocumentPipeline& pipeline) : pipeline_(pipeline) {}

        DocumentPipeline& pipeline_;
    };

    DocumentPipeline(ResourceStore* resource_store, IResourceProvider* resource_provider, IImageDecoder* image_decoder,
                     std::unique_ptr<IScriptEngine> script_engine);
    ~DocumentPipeline();

    DocumentPipeline(const DocumentPipeline&) = delete;
    DocumentPipeline& operator=(const DocumentPipeline&) = delete;
    DocumentPipeline(DocumentPipeline&&) = delete;
    DocumentPipeline& operator=(DocumentPipeline&&) = delete;

    LayoutApi layout() { return LayoutApi(*this); }
    InteractionApi interaction() { return InteractionApi(*this); }
    void reset();

private:
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
    bool set_control_interaction_at(const HitTestContext& context);
    bool clear_control_interaction();
    bool has_focused_input() const;
    InputEditResult handle_text_input(std::string_view text);
    InputEditResult handle_key_down(const InputEvent& event, std::string_view base_url);
    std::optional<std::string> focused_input_value() const;

    bool has_dom_tree() const;
    bool has_render_tree() const;
    float content_height() const;
    size_t render_tree_children() const;
    const std::vector<std::string>& stylesheet_links() const;
    const std::vector<std::string>& image_links() const;
    const std::vector<std::string>& background_image_links() const;

    std::unique_ptr<DocumentResources> resources_;
    std::unique_ptr<DocumentModel> model_;
    std::unique_ptr<DocumentInteraction> interaction_;
    std::unique_ptr<DocumentRenderer> renderer_;
    std::unique_ptr<DocumentStyleCoordinator> style_coordinator_;
    std::unique_ptr<DocumentScripting> scripting_;
};

}  // namespace Hummingbird::Engine
