#include "app/DocumentEventRouter.h"

#include <algorithm>

#include "app/BrowserApp.h"
#include "app/RenderCoordinator.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "engine/forms/FormSubmission.h"
#include "engine/tab/Tab.h"

namespace Hummingbird::App {

bool DocumentEventRouter::handle_text_input(const Hummingbird::InputEvent& event) {
    if (app_.active_tab().interaction().handle_text_input(event.text.text)) {
        app_.render_coordinator_->set_controls_dirty();
        return true;
    }
    return false;
}

bool DocumentEventRouter::handle_key_down(const Hummingbird::InputEvent& event) {
    auto tab_result = app_.active_tab().interaction().handle_key_down(event);
    if (!tab_result.handled) {
        return false;
    }
    if (tab_result.submitted_form) {
        app_.navigate_and_reflect_submission(*tab_result.submitted_form);
    }
    if (tab_result.needs_repaint) {
        app_.render_coordinator_->set_controls_dirty();
    }
    return true;
}

void DocumentEventRouter::handle_mouse_down(const Hummingbird::InputEvent& event) {
    app_.browser_chrome_.url_bar().set_active(false, app_.window_.get(), nullptr);
    app_.render_coordinator_->set_chrome_dirty();

    if (!app_.graphics_ || !app_.window_) {
        return;
    }

    auto [win_w, win_h] = app_.window_->get_size();
    const auto viewport = app_.compute_content_viewport(win_w, win_h);
    Hummingbird::Layout::Point point{static_cast<float>(event.mouse_button.x),
                                     static_cast<float>(event.mouse_button.y)};
    HB_LOG_DEBUG("[input] mouse click at (" << point.x << "," << point.y << ") viewport=(" << viewport.x << ","
                                            << viewport.y << "," << viewport.width << "," << viewport.height << ")");
    auto interaction = app_.active_tab().interaction();
    auto click_result = interaction.dispatch_click(point, viewport, *app_.graphics_);
    if (click_result.mutated) {
        app_.render_coordinator_->set_document_dirty();
    }
    bool interaction_changed = interaction.set_control_interaction_at(point, viewport);
    bool was_focused = interaction.has_focused_input();
    bool now_focused = interaction.focus_input_at(point, viewport);
    HB_LOG_DEBUG("[input] focus probe was=" << was_focused << " now=" << now_focused);
    // URL bar deactivation stops platform text input. Re-enable it here whenever
    // a document input is focused, even if focus state did not transition.
    if (app_.window_) {
        if (now_focused) {
            app_.window_->start_text_input();
        } else {
            app_.window_->stop_text_input();
        }
    }
    if (app_.tab_text_input_active_ != now_focused || was_focused != now_focused) {
        app_.tab_text_input_active_ = now_focused;
        app_.render_coordinator_->set_controls_dirty();
    }
    if (interaction_changed || was_focused || now_focused) {
        if (interaction.refresh_styles_for_interaction(*app_.graphics_, viewport)) {
            app_.mark_document_and_controls_dirty();
        }
    }
    if (now_focused) {
        return;
    }
    (void)handle_document_hit_navigation(point, viewport);
}

void DocumentEventRouter::handle_mouse_wheel(const Hummingbird::InputEvent& event) {
    const float delta = static_cast<float>(event.wheel.dy) * 32.0f;

    auto [win_w, win_h] = app_.window_->get_size();
    const float viewport_h = static_cast<float>(
        std::max(0, win_h - app_.browser_chrome_.url_bar().height() - app_.browser_chrome_.tab_strip_height()));
    app_.active_tab().scroll_by(delta, viewport_h);

    app_.render_coordinator_->set_document_dirty();
}

bool DocumentEventRouter::handle_document_hit_navigation(const Hummingbird::Layout::Point& point,
                                                         const Hummingbird::Layout::Rect& viewport) {
    auto interaction = app_.active_tab().interaction();
    auto submit = interaction.submit_form_at(point, viewport);
    if (submit) {
        HB_LOG_DEBUG("[input] submit hit method="
                     << (submit->method == Hummingbird::Engine::FormSubmitMethod::Post ? "POST" : "GET")
                     << " url=" << submit->url);
        app_.browser_chrome_.url_bar().set_text(submit->url);
        app_.navigate_and_reflect_submission(*submit);
        return true;
    }

    auto link = interaction.hit_test_link(point, viewport);
    if (!link) {
        return false;
    }
    app_.browser_chrome_.url_bar().set_text(*link);
    app_.navigate_and_reflect_url(*link);
    return true;
}

}  // namespace Hummingbird::App
