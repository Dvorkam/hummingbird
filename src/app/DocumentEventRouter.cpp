#include "app/DocumentEventRouter.h"

#include "app/BrowserApp.h"
#include "app/BrowserChrome.h"
#include "app/RenderCoordinator.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "engine/forms/FormSubmission.h"
#include "engine/tab/Tab.h"

namespace Hummingbird::App {

bool DocumentEventRouter::handle_text_input(const Hummingbird::InputEvent& event) {
    if (app_.active_tab().handle_text_input(event.text.text)) {
        render_.set_controls_dirty();
        return true;
    }
    return false;
}

bool DocumentEventRouter::handle_key_down(const Hummingbird::InputEvent& event) {
    auto tab_result = app_.active_tab().handle_key_down(event);
    if (!tab_result.handled) {
        return false;
    }
    if (tab_result.submitted_form) {
        app_.navigate_and_reflect_submission(*tab_result.submitted_form);
    }
    if (tab_result.needs_repaint) {
        render_.set_controls_dirty();
    }
    return true;
}

void DocumentEventRouter::handle_mouse_down(const Hummingbird::InputEvent& event) {
    chrome_.url_bar().set_active(false, window_, nullptr);
    render_.set_chrome_dirty();

    if (!graphics_ || !window_) {
        return;
    }

    auto [win_w, win_h] = window_->get_size();
    const auto viewport = chrome_.content_viewport(win_w, win_h);
    Hummingbird::Layout::Point point{static_cast<float>(event.mouse_button.x),
                                     static_cast<float>(event.mouse_button.y)};
    HB_LOG_DEBUG("[input] mouse click at (" << point.x << "," << point.y << ") viewport=(" << viewport.x << ","
                                            << viewport.y << "," << viewport.width << "," << viewport.height << ")");
    auto& tab = app_.active_tab();
    // F1 debug mode: log the element under the cursor to the console instead of
    // cluttering the render (T-DEBUG-INSPECT-1).
    if (render_.debug_outlines()) {
        if (auto info = tab.inspect_at(point, viewport)) {
            HB_LOG_INFO("[inspect] " << *info);
        }
    }
    auto click_result = tab.dispatch_click(point, viewport, *graphics_);
    if (click_result.mutated) {
        render_.set_document_dirty();
    }
    bool interaction_changed = tab.set_control_interaction_at(point, viewport);
    bool was_focused = tab.has_focused_input();
    bool now_focused = tab.focus_input_at(point, viewport);
    HB_LOG_DEBUG("[input] focus probe was=" << was_focused << " now=" << now_focused);
    // URL bar deactivation stops platform text input. Re-enable it here whenever
    // a document input is focused, even if focus state did not transition.
    if (window_) {
        if (now_focused) {
            window_->start_text_input();
        } else {
            window_->stop_text_input();
        }
    }
    if (app_.tab_text_input_active() != now_focused || was_focused != now_focused) {
        app_.set_tab_text_input_active(now_focused);
        render_.set_controls_dirty();
    }
    if (interaction_changed || was_focused || now_focused) {
        if (tab.refresh_styles_for_interaction(*graphics_, viewport)) {
            render_.set_document_and_controls_dirty();
        }
    }
    if (now_focused) {
        return;
    }
    (void)handle_document_hit_navigation(point, viewport);
}

void DocumentEventRouter::handle_mouse_wheel(const Hummingbird::InputEvent& event) {
    const float delta = static_cast<float>(event.wheel.dy) * 32.0f;

    auto [win_w, win_h] = window_->get_size();
    const float viewport_h = chrome_.content_viewport(win_w, win_h).height;
    app_.active_tab().scroll_by(delta, viewport_h);

    render_.set_document_dirty();
}

bool DocumentEventRouter::handle_document_hit_navigation(const Hummingbird::Layout::Point& point,
                                                         const Hummingbird::Layout::Rect& viewport) {
    auto& tab = app_.active_tab();
    auto submit = tab.submit_form_at(point, viewport);
    if (submit) {
        HB_LOG_DEBUG("[input] submit hit method="
                     << (submit->method == Hummingbird::Engine::FormSubmitMethod::Post ? "POST" : "GET")
                     << " url=" << submit->url);
        app_.navigate_and_reflect_submission(*submit);
        return true;
    }

    auto link = tab.hit_test_link(point, viewport);
    if (!link) {
        return false;
    }
    app_.navigate_and_reflect_url(*link);
    return true;
}

}  // namespace Hummingbird::App
