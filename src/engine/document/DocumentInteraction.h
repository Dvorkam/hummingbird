#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "engine/document/DocumentInputController.h"
#include "engine/document/DocumentNavigation.h"
#include "engine/forms/FormSubmission.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentModel;

class DocumentInteraction {
public:
    struct InputEditResult {
        bool handled = false;
        bool needs_repaint = false;
        std::optional<FormSubmission> submitted_form;
    };

    struct HitTestContext {
        Layout::Point point;
        Layout::Rect viewport;
        std::string_view base_url;
        float scroll_y = 0.0f;
    };

    explicit DocumentInteraction(DocumentModel& model);

    void reset();

    std::optional<std::string> hit_test_link(const HitTestContext& context) const;
    std::optional<FormSubmission> submit_form_at(const HitTestContext& context) const;
    // Returns a human-readable description (tag/id/classes, geometry, key computed
    // style) of the topmost element under the point, for F1 debug inspection
    // (T-DEBUG-INSPECT-1). nullopt when no element is hit.
    std::optional<std::string> inspect_at(const HitTestContext& context) const;

    bool focus_input_at(const Layout::RenderObject* render_tree, const HitTestContext& context);
    bool focus_autofocus_input(const Layout::RenderObject* render_tree);
    bool clear_input_focus();

    bool set_control_interaction_at(const Layout::RenderObject* render_tree, const HitTestContext& context);
    bool clear_control_interaction();

    bool has_focused_input() const { return input_controller_.has_focus(); }
    std::optional<std::string> focused_input_value() const { return input_controller_.focused_value(); }

    InputEditResult handle_text_input(std::string_view text);
    InputEditResult handle_key_down(const InputEvent& event, std::string_view base_url);

    void paint_controls(const Layout::RenderObject* render_tree, IGraphicsContext& graphics,
                        const Layout::Rect& viewport, float scroll_y, bool repaint_background) const;

    const DocumentInputController& input_controller() const { return input_controller_; }

private:
    DocumentModel& model_;
    DocumentNavigation navigation_;
    DocumentInputController input_controller_;
};

}  // namespace Hummingbird::Engine
