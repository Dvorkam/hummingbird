#include "engine/document/DocumentInteraction.h"

#include <sstream>

#include "core/dom/Element.h"
#include "engine/document/DocumentModel.h"
#include "html/HtmlAttributeNames.h"
#include "layout/RenderObject.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/PositioningUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Engine {

namespace {
const char* display_name(Css::ComputedStyle::Display display) {
    switch (display) {
        case Css::ComputedStyle::Display::Block:
            return "block";
        case Css::ComputedStyle::Display::Inline:
            return "inline";
        case Css::ComputedStyle::Display::InlineBlock:
            return "inline-block";
        case Css::ComputedStyle::Display::ListItem:
            return "list-item";
        case Css::ComputedStyle::Display::Flex:
            return "flex";
        case Css::ComputedStyle::Display::Grid:
            return "grid";
        case Css::ComputedStyle::Display::None:
            return "none";
    }
    return "?";
}

const char* position_name(Css::ComputedStyle::Position position) {
    switch (position) {
        case Css::ComputedStyle::Position::Static:
            return "static";
        case Css::ComputedStyle::Position::Relative:
            return "relative";
        case Css::ComputedStyle::Position::Absolute:
            return "absolute";
    }
    return "?";
}

std::string format_optional_length(const std::optional<Css::ComputedStyle::LengthValue>& length) {
    if (!length) {
        return "auto";
    }
    std::ostringstream os;
    if (length->has_percent) {
        os << length->percent << "%";
        if (length->px != 0.0f) {
            os << (length->px < 0.0f ? "" : "+") << length->px << "px";
        }
    } else {
        os << length->px << "px";
    }
    return os.str();
}

std::string describe(const Layout::RenderObject& node, const Layout::Rect& absolute) {
    std::ostringstream os;
    const auto* element = dynamic_cast<const DOM::Element*>(node.get_dom_node());
    os << "<" << (element ? element->get_tag_name() : std::string("#text")) << ">";
    if (element) {
        if (const auto* id = element->find_attribute(Hummingbird::Html::AttributeNames::Id); id && !id->empty()) {
            os << " #" << *id;
        }
        if (const auto* classes = element->find_attribute(Hummingbird::Html::AttributeNames::Class);
            classes && !classes->empty()) {
            os << " class='" << *classes << "'";
        }
    }
    os << " rect=(" << absolute.x << "," << absolute.y << " " << absolute.width << "x" << absolute.height << ")";
    if (const auto* style = node.get_computed_style()) {
        os << " display=" << display_name(style->display) << " position=" << position_name(style->position);
        os << " width=" << format_optional_length(style->width) << " height=" << format_optional_length(style->height);
        os << " margin=(" << style->margin.top << " " << style->margin.right << " " << style->margin.bottom << " "
           << style->margin.left << ")";
        os << " padding=(" << style->padding.top << " " << style->padding.right << " " << style->padding.bottom << " "
           << style->padding.left << ")";
    }
    return os.str();
}
}  // namespace

DocumentInteraction::DocumentInteraction(DocumentModel& model) : model_(model), navigation_(model) {}

std::optional<std::string> DocumentInteraction::inspect_at(const HitTestContext& context) const {
    const auto* render_tree = model_.render_tree();
    if (!render_tree || !Layout::rect_contains_point(context.viewport, context.point)) {
        return std::nullopt;
    }

    const Layout::RenderObject* hit = nullptr;
    Layout::Rect hit_absolute{};
    const Layout::Point offset{0.0f, -context.scroll_y};
    Layout::Positioning::traverse_render_tree_z_order(
        *render_tree, offset,
        [&](const Layout::RenderObject& node, const Layout::Rect& absolute, const Layout::Point&) {
            if (!Layout::rect_intersects(absolute, context.viewport) ||
                !Layout::rect_contains_point(absolute, context.point)) {
                if (node.has_absolute_descendant()) {
                    return Layout::Traversal::TraverseAction::Continue;
                }
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        [&](const Layout::RenderObject& node, const Layout::Rect& absolute, const Layout::Point&) {
            if (!Layout::rect_contains_point(absolute, context.point)) {
                return Layout::Traversal::TraverseAction::Continue;
            }
            // Topmost element under the cursor wins (reverse child order visits
            // painted-on-top boxes first in the exit phase).
            if (dynamic_cast<const DOM::Element*>(node.get_dom_node())) {
                hit = &node;
                hit_absolute = absolute;
                return Layout::Traversal::TraverseAction::Stop;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        Layout::Traversal::ChildOrder::Reverse);

    if (!hit) {
        return std::nullopt;
    }
    return describe(*hit, hit_absolute);
}

void DocumentInteraction::reset() {
    input_controller_.reset();
}

std::optional<std::string> DocumentInteraction::hit_test_link(const HitTestContext& context) const {
    return navigation_.hit_test_link(model_.render_tree(), context.point, context.viewport, context.scroll_y,
                                     context.base_url);
}

std::optional<FormSubmission> DocumentInteraction::submit_form_at(const HitTestContext& context) const {
    return navigation_.submit_form_at(model_.render_tree(), context.point, context.viewport, context.scroll_y,
                                      context.base_url);
}

bool DocumentInteraction::focus_input_at(const Layout::RenderObject* render_tree, const HitTestContext& context) {
    return input_controller_.focus_input_at(render_tree, context.point, context.viewport, context.scroll_y);
}

bool DocumentInteraction::focus_autofocus_input(const Layout::RenderObject* render_tree) {
    return input_controller_.focus_autofocus_input(render_tree);
}

bool DocumentInteraction::clear_input_focus() {
    return input_controller_.clear_focus();
}

bool DocumentInteraction::set_control_interaction_at(const Layout::RenderObject* render_tree,
                                                     const HitTestContext& context) {
    return input_controller_.set_control_interaction_at(render_tree, context.point, context.viewport, context.scroll_y);
}

bool DocumentInteraction::clear_control_interaction() {
    return input_controller_.clear_control_interaction();
}

DocumentInteraction::InputEditResult DocumentInteraction::handle_text_input(std::string_view text) {
    auto result = input_controller_.handle_text_input(text);
    return {result.handled, result.needs_repaint, std::nullopt};
}

DocumentInteraction::InputEditResult DocumentInteraction::handle_key_down(const InputEvent& event,
                                                                          std::string_view base_url) {
    auto result = input_controller_.handle_key_down(event);
    InputEditResult output{result.handled, result.needs_repaint, std::nullopt};

    if (event.key.key == Key::Enter && input_controller_.has_focus()) {
        const auto* focused = input_controller_.focused_element();
        if (focused) {
            output.submitted_form = model_.build_form_submission(*focused, base_url);
        }
        output.handled = true;
    }

    return output;
}

void DocumentInteraction::paint_controls(const Layout::RenderObject* render_tree, IGraphicsContext& graphics,
                                         const Layout::Rect& viewport, float scroll_y, bool repaint_background) const {
    input_controller_.paint_controls(render_tree, graphics, viewport, scroll_y, repaint_background);
}

}  // namespace Hummingbird::Engine
