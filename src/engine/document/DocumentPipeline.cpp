#include "engine/document/DocumentPipeline.h"

#include <cstdlib>
#include <memory>
#include <ostream>
#include <utility>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "engine/document/DocumentInputUtils.h"
#include "engine/document/DocumentInteraction.h"
#include "engine/document/DocumentModel.h"
#include "engine/document/DocumentRenderer.h"
#include "engine/document/DocumentResources.h"
#include "engine/document/DocumentScripting.h"
#include "engine/document/DocumentStyleCoordinator.h"

namespace Hummingbird::Engine {

namespace {
bool relayout_debug_enabled() {
    static const bool enabled = std::getenv("HB_DEBUG_LAYOUT_STATE") != nullptr;
    return enabled;
}

// Maps a platform key to its DOM `key`/`code` values (the subset the InputEvent
// Key enum carries). `key` reflects Shift for letters; `code` is layout-agnostic.
struct KeyFields {
    std::string key;
    std::string code;
};
KeyFields key_fields(const InputEvent& event) {
    const int value = static_cast<int>(event.key.key);
    if (value >= static_cast<int>(Key::A) && value <= static_cast<int>(Key::Z)) {
        const int index = value - static_cast<int>(Key::A);
        const char upper = static_cast<char>('A' + index);
        const char letter = event.mods.shift ? upper : static_cast<char>('a' + index);
        return {std::string(1, letter), std::string("Key") + upper};
    }
    switch (event.key.key) {
        case Key::Enter:
            return {"Enter", "Enter"};
        case Key::Escape:
            return {"Escape", "Escape"};
        case Key::Backspace:
            return {"Backspace", "Backspace"};
        case Key::Delete:
            return {"Delete", "Delete"};
        case Key::Insert:
            return {"Insert", "Insert"};
        case Key::Home:
            return {"Home", "Home"};
        case Key::End:
            return {"End", "End"};
        case Key::Left:
            return {"ArrowLeft", "ArrowLeft"};
        case Key::Right:
            return {"ArrowRight", "ArrowRight"};
        case Key::F1:
            return {"F1", "F1"};
        default:
            return {"", ""};
    }
}
}  // namespace

DocumentPipeline::DocumentPipeline(ResourceStore* resource_store, IResourceProvider* resource_provider,
                                   IImageDecoder* image_decoder, std::unique_ptr<IScriptEngine> script_engine)
    : resources_(std::make_unique<DocumentResources>(resource_store, resource_provider, image_decoder)),
      model_(std::make_unique<DocumentModel>()),
      interaction_(std::make_unique<DocumentInteraction>(*model_)),
      renderer_(std::make_unique<DocumentRenderer>(*model_, *interaction_)),
      style_coordinator_(std::make_unique<DocumentStyleCoordinator>(*model_, *resources_)),
      scripting_(std::make_unique<DocumentScripting>(std::move(script_engine))) {
    // Bridge JS focus()/blur() to the input controller's caret target (7.2.6).
    scripting_->set_focus_sink([this](DOM::Element* element, bool focused) {
        interaction_->apply_script_focus(element, focused);
        // Keep the change-detection baseline in sync: a later blur compares the
        // field's value against this snapshot to decide whether `change` fires.
        // (Dispatching focus/blur events from here is T-FOCUS-EVENTS-FROM-JS-1 —
        // blocked on making event dispatch re-entrant.)
        const DOM::Element* focused_now = interaction_->input_controller().focused_element();
        focus_snapshot_value_ = focused_now ? input_value(*focused_now) : std::string{};
    });
}

DocumentPipeline::~DocumentPipeline() = default;

void DocumentPipeline::reset() {
    // Scripting teardown must run while the DOM arena is still alive: it
    // destroys the host's detached nodes (whose storage lives in the arena)
    // and neutralizes JS wrappers before model_->reset() frees the arena.
    scripting_->reset();
    interaction_->reset();
    model_->reset();
    renderer_->reset();
}

bool DocumentPipeline::parse_html(std::string_view html) {
    auto result = model_->parse_html(html);
    if (!result.ok && result.arena_failed) {
        reset();
    }
    return result.ok;
}

bool DocumentPipeline::run_scripts(const ExternalScriptLookup& external_lookup) {
    return scripting_->run_document_scripts(*model_, external_lookup);
}

std::vector<std::string> DocumentPipeline::external_script_srcs() const {
    std::vector<std::string> srcs;
    for (const auto& script : model_->document_scripts()) {
        if (script.is_external()) {
            srcs.push_back(script.src);
        }
    }
    return srcs;
}

void DocumentPipeline::set_extension_style_blocks(const std::vector<std::string>& style_blocks) {
    style_coordinator_->set_extension_style_blocks(style_blocks);
}

DocumentPipeline::ScriptDispatchResult DocumentPipeline::dispatch_click(const HitTestContext& context) {
    auto result =
        scripting_->dispatch_click(*model_, context.viewport, context.point, context.scroll_y, context.click_count);
    return {result.handled, result.mutated, result.default_prevented};
}

DocumentPipeline::ScriptDispatchResult DocumentPipeline::dispatch_load() {
    auto result = scripting_->dispatch_load(*model_);
    return {result.handled, result.mutated};
}

void DocumentPipeline::mark_url_visited(std::string_view url) {
    if (!url.empty()) {
        visited_urls_.emplace(url);
    }
}

void DocumentPipeline::apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                               std::string_view base_url) {
    const Css::MediaContext media{viewport.width, viewport.height};
    // Flag anchors whose target has been visited before styling, so `:visited`
    // and the vlink color resolve during this apply (T-HIST-1).
    model_->mark_visited_links(visited_urls_, base_url);
    if (!style_coordinator_->apply_styles_and_build(base_url, media)) {
        return;
    }

    update_image_resources(base_url);
    // One completed style+layout pass. The counter proves batching: a task that
    // makes N DOM mutations produces exactly one pass, not N (7.4.1).
    ++style_layout_passes_;
    relayout(graphics, viewport);
}

bool DocumentPipeline::rebuild_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                          std::string_view base_url) {
    apply_styles_and_layout(graphics, viewport, base_url);
    return model_->has_render_tree();
}

bool DocumentPipeline::update_image_resources(std::string_view base_url) {
    return style_coordinator_->update_image_resources(base_url);
}

bool DocumentPipeline::needs_restyle_for_viewport(const Layout::Rect& viewport) const {
    return model_->media_conditions_change({viewport.width, viewport.height});
}

void DocumentPipeline::relayout(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    renderer_->relayout(graphics, viewport);
    if (relayout_debug_enabled()) {
        HB_LOG_WARN("[layout-debug] relayout content_h=" << renderer_->content_height());
    }
}

void DocumentPipeline::paint(IGraphicsContext& graphics, const PaintContext& context) {
    renderer_->paint(graphics, {context.viewport, context.debug_outlines, context.scroll_y});
}

void DocumentPipeline::paint_controls(IGraphicsContext& graphics, const PaintContext& context,
                                      bool repaint_background) {
    renderer_->paint_controls(graphics, {context.viewport, context.debug_outlines, context.scroll_y},
                              repaint_background);
}

std::optional<std::string> DocumentPipeline::hit_test_link(const HitTestContext& context) const {
    return interaction_->hit_test_link({context.point, context.viewport, context.base_url, context.scroll_y});
}

std::optional<std::string> DocumentPipeline::inspect_at(const HitTestContext& context) const {
    return interaction_->inspect_at({context.point, context.viewport, context.base_url, context.scroll_y});
}

std::optional<FormSubmission> DocumentPipeline::submit_form_at(const HitTestContext& context) const {
    return interaction_->submit_form_at({context.point, context.viewport, context.base_url, context.scroll_y});
}

bool DocumentPipeline::focus_input_at(const HitTestContext& context) {
    DOM::Element* before = const_cast<DOM::Element*>(interaction_->input_controller().focused_element());
    bool result = interaction_->focus_input_at(model_->render_tree(),
                                               {context.point, context.viewport, context.base_url, context.scroll_y});
    DOM::Element* after = const_cast<DOM::Element*>(interaction_->input_controller().focused_element());
    if (before != after) {
        fire_focus_transition(before, after);
    }
    return result;
}

bool DocumentPipeline::focus_autofocus_input() {
    bool result = interaction_->focus_autofocus_input(model_->render_tree());
    if (result) {
        fire_focus_transition(nullptr, const_cast<DOM::Element*>(interaction_->input_controller().focused_element()));
    }
    return result;
}

bool DocumentPipeline::clear_input_focus() {
    DOM::Element* before = const_cast<DOM::Element*>(interaction_->input_controller().focused_element());
    bool result = interaction_->clear_input_focus();
    if (before) {
        fire_focus_transition(before, nullptr);
    }
    return result;
}

bool DocumentPipeline::set_control_interaction_at(const HitTestContext& context) {
    return interaction_->set_control_interaction_at(
        model_->render_tree(), {context.point, context.viewport, context.base_url, context.scroll_y});
}

bool DocumentPipeline::clear_control_interaction() {
    return interaction_->clear_control_interaction();
}

bool DocumentPipeline::has_focused_input() const {
    return interaction_->has_focused_input();
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_text_input(std::string_view text) {
    const DOM::Element* focused = interaction_->input_controller().focused_element();
    std::string before = focused ? input_value(*focused) : std::string{};
    auto result = interaction_->handle_text_input(text);
    bool mutated = false;
    if (focused && input_value(*focused) != before) {
        // The field's value changed: fire a bubbling, non-cancelable `input`.
        mutated = dispatch_element_event(const_cast<DOM::Element*>(focused), "input", true, false).mutated;
    }
    return {result.handled, result.needs_repaint, mutated, std::move(result.submitted_form)};
}

DocumentPipeline::KeyDispatchResult DocumentPipeline::dispatch_element_event(DOM::Node* target, const char* type,
                                                                             bool bubbles, bool cancelable) {
    if (!target) {
        return {};
    }
    ScriptDomEvent event{type, bubbles, cancelable, "", ""};
    auto result = scripting_->dispatch_dom_event(*model_, target, event);
    return {result.mutated, result.default_prevented};
}

bool DocumentPipeline::fire_focus_transition(DOM::Element* before, DOM::Element* after) {
    bool mutated = false;
    if (before) {
        // `change` fires only when the value was edited during this focus.
        if (input_value(*before) != focus_snapshot_value_) {
            mutated |= dispatch_element_event(before, "change", /*bubbles*/ true, /*cancelable*/ false).mutated;
        }
        mutated |= dispatch_element_event(before, "blur", /*bubbles*/ false, /*cancelable*/ false).mutated;
    }
    if (after) {
        focus_snapshot_value_ = input_value(*after);
        mutated |= dispatch_element_event(after, "focus", /*bubbles*/ false, /*cancelable*/ false).mutated;
    } else {
        focus_snapshot_value_.clear();
    }
    return mutated;
}

DocumentPipeline::KeyDispatchResult DocumentPipeline::dispatch_key_event(const char* type, const InputEvent& event) {
    // Keyboard events target the focused element, or the document (dom root) when
    // nothing is focused. Bubbling + cancelable, with key/code populated.
    const DOM::Element* focused = interaction_->input_controller().focused_element();
    DOM::Node* target = focused ? const_cast<DOM::Element*>(focused) : model_->dom_root();
    if (!target) {
        return {};
    }
    KeyFields fields = key_fields(event);
    ScriptDomEvent dom_event{type, /*bubbles*/ true, /*cancelable*/ true, fields.key, fields.code};
    auto result = scripting_->dispatch_dom_event(*model_, target, dom_event);
    return {result.mutated, result.default_prevented};
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_key_down(const InputEvent& event,
                                                                    std::string_view base_url) {
    // Fire the DOM keydown first; preventDefault suppresses the engine's default
    // key handling (text-edit / Enter-to-submit).
    auto dom = dispatch_key_event("keydown", event);
    if (dom.default_prevented) {
        return {/*handled*/ true, /*needs_repaint*/ dom.mutated, /*mutated*/ dom.mutated, std::nullopt};
    }
    const DOM::Element* focused = interaction_->input_controller().focused_element();
    std::string before = focused ? input_value(*focused) : std::string{};
    auto result = interaction_->handle_key_down(event, base_url);
    bool mutated = dom.mutated;
    if (focused && input_value(*focused) != before) {
        // An editing key (backspace/delete) changed the value: fire `input`.
        mutated |= dispatch_element_event(const_cast<DOM::Element*>(focused), "input", true, false).mutated;
    }
    return {result.handled || mutated, result.needs_repaint || mutated, mutated, std::move(result.submitted_form)};
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_key_up(const InputEvent& event) {
    auto dom = dispatch_key_event("keyup", event);
    return {dom.mutated, dom.mutated, dom.mutated, std::nullopt};
}

void DocumentPipeline::set_location(std::string_view url) {
    scripting_->set_location(url);
}

DocumentPipeline::FragmentNavResult DocumentPipeline::navigate_fragment(std::string_view url) {
    auto result = scripting_->navigate_fragment(*model_, url);
    return {result.handled, result.mutated};
}

DocumentPipeline::TimerRunResult DocumentPipeline::run_timers(double now_ms) {
    auto result = scripting_->run_timers(*model_, now_ms);
    return {result.handled, result.mutated};
}

bool DocumentPipeline::has_pending_timers() const {
    return scripting_->has_pending_timers();
}

std::optional<std::string> DocumentPipeline::consume_location_change() {
    return scripting_->consume_location_change();
}

DocumentPipeline::SubmitDispatchResult DocumentPipeline::dispatch_submit(const DOM::Element* form) {
    if (!form) {
        return {};
    }
    auto result = dispatch_element_event(const_cast<DOM::Element*>(form), "submit", /*bubbles*/ true,
                                         /*cancelable*/ true);
    return {result.default_prevented, result.mutated};
}

std::optional<std::string> DocumentPipeline::focused_input_value() const {
    return interaction_->focused_input_value();
}

const Layout::RenderObject* DocumentPipeline::render_root() const {
    return model_->render_tree();
}

bool DocumentPipeline::has_dom_tree() const {
    return model_->has_dom_tree();
}

bool DocumentPipeline::has_render_tree() const {
    return model_->has_render_tree();
}

float DocumentPipeline::content_height() const {
    return renderer_->content_height();
}

size_t DocumentPipeline::render_tree_children() const {
    return model_->render_tree_children();
}

const std::vector<std::string>& DocumentPipeline::stylesheet_links() const {
    return model_->stylesheet_links();
}

const std::vector<std::string>& DocumentPipeline::image_links() const {
    return model_->image_links();
}

const std::vector<std::string>& DocumentPipeline::background_image_links() const {
    return model_->background_image_links();
}

const std::vector<std::string>& DocumentPipeline::font_requests() const {
    return model_->font_requests();
}

}  // namespace Hummingbird::Engine
