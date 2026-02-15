#include "app/ChromeEventRouter.h"

#include "app/BrowserApp.h"
#include "app/RenderCoordinator.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "engine/tab/Tab.h"

namespace Hummingbird::App {

bool ChromeEventRouter::handle_text_input(const Hummingbird::InputEvent& event) {
    if (app_.browser_chrome_.url_bar().handle_text_input(event.text.text)) {
        app_.render_coordinator_->set_chrome_dirty();
        return true;
    }
    return false;
}

bool ChromeEventRouter::handle_key_down(const Hummingbird::InputEvent& event) {
    if (handle_tab_shortcut(event)) {
        return true;
    }

    if (handle_global_key_shortcut(event)) {
        return true;
    }

    if (handle_url_bar_key_down(event)) {
        return true;
    }

    return false;
}

bool ChromeEventRouter::handle_mouse_down(const Hummingbird::InputEvent& event) {
    if (handle_tab_strip_mouse_down(event)) {
        return true;
    }
    if (handle_url_bar_mouse_down(event)) {
        return true;
    }
    return false;
}

bool ChromeEventRouter::handle_tab_shortcut(const Hummingbird::InputEvent& event) {
    if (!event.key.repeat) {
        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::T) {
            if (app_.new_tab()) {
                app_.mark_all_layers_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::W) {
            if (app_.close_active_tab()) {
                app_.mark_all_layers_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::Right) {
            if (app_.activate_next_tab()) {
                app_.mark_all_layers_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::Left) {
            if (app_.activate_prev_tab()) {
                app_.mark_all_layers_dirty();
            }
            return true;
        }
    }
    return false;
}

bool ChromeEventRouter::handle_global_key_shortcut(const Hummingbird::InputEvent& event) {
    if (event.key.key == Key::L && event.mods.ctrl && event.mods.shift) {
        HB_LOG_INFO("[ui] Forcing document repaint");
        app_.render_coordinator_->set_document_dirty();
        return true;
    }

    if (event.key.key == Key::L && event.mods.ctrl) {
        app_.browser_chrome_.url_bar().set_active(true, app_.window_.get(), "[ui] URL bar focused");
        app_.browser_chrome_.url_bar().move_caret_to_end();
        if (app_.active_tab().interaction().clear_input_focus()) {
            app_.render_coordinator_->set_controls_dirty();
        }
        app_.render_coordinator_->set_chrome_dirty();
        return true;
    }

    if (event.key.key == Key::F1) {
        app_.debug_outlines_ = !app_.debug_outlines_;
        HB_LOG_INFO("[ui] Debug outlines " << (app_.debug_outlines_ ? "ON" : "OFF"));
        app_.render_coordinator_->set_document_dirty();
        return true;
    }
    return false;
}

bool ChromeEventRouter::handle_url_bar_key_down(const Hummingbird::InputEvent& event) {
    auto result = app_.browser_chrome_.url_bar().handle_key_down(event, app_.window_.get());
    if (!result.handled) {
        return false;
    }
    if (result.submitted_url) {
        app_.navigate_and_reflect_url(*result.submitted_url);
    }
    if (result.needs_repaint) {
        app_.render_coordinator_->set_chrome_dirty();
    }
    return true;
}

bool ChromeEventRouter::handle_tab_strip_mouse_down(const Hummingbird::InputEvent& event) {
    if (!app_.graphics_ || !app_.window_) {
        return false;
    }
    const int tabs_top_y = app_.browser_chrome_.url_bar().height();
    const int tabs_bottom_y = app_.browser_chrome_.url_bar().height() + app_.browser_chrome_.tab_strip_height();
    if (event.mouse_button.y < tabs_top_y || event.mouse_button.y >= tabs_bottom_y) {
        return false;
    }
    const auto [win_w, win_h] = app_.window_->get_size();
    (void)win_h;
    auto result = app_.browser_chrome_.handle_tab_strip_mouse_down(event.mouse_button.x, event.mouse_button.y, win_w,
                                                                   tabs_top_y, app_.tab_controller_.manager());
    if (!result.handled) {
        return false;
    }
    if (result.activated_tab && app_.tab_controller_.set_active(*result.activated_tab)) {
        app_.on_active_tab_changed();
    }
    app_.mark_all_layers_dirty();
    return true;
}

bool ChromeEventRouter::handle_url_bar_mouse_down(const Hummingbird::InputEvent& event) {
    auto url_result = app_.browser_chrome_.url_bar().handle_mouse_down(event.mouse_button.x, event.mouse_button.y,
                                                                       app_.window_.get());
    if (!url_result.handled) {
        return false;
    }

    auto interaction = app_.active_tab().interaction();
    bool interaction_state_changed = false;
    interaction_state_changed |= interaction.clear_control_interaction();
    interaction_state_changed |= interaction.clear_input_focus();
    if (interaction_state_changed && app_.graphics_ && app_.window_) {
        auto [w, h] = app_.window_->get_size();
        const auto viewport = app_.compute_content_viewport(w, h);
        if (interaction.refresh_styles_for_interaction(*app_.graphics_, viewport)) {
            app_.mark_document_and_controls_dirty();
        }
    } else if (interaction_state_changed) {
        app_.mark_document_and_controls_dirty();
    }
    if (url_result.security_override_requested) {
        if (app_.active_tab().allow_insecure_for_current_host()) {
            app_.navigate_and_reflect_url(app_.active_tab().requested_url());
        }
    }
    app_.tab_text_input_active_ = false;
    if (url_result.needs_repaint) {
        app_.render_coordinator_->set_chrome_dirty();
    }
    return true;
}

}  // namespace Hummingbird::App
