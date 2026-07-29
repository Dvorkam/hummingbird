#include "app/ChromeEventRouter.h"

#include <string>

#include "app/BrowserApp.h"
#include "app/BrowserChrome.h"
#include "app/RenderCoordinator.h"
#include "app/TabController.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "engine/tab/Tab.h"

namespace Hummingbird::App {

bool ChromeEventRouter::handle_text_input(const Hummingbird::InputEvent& event) {
    if (chrome_.url_bar().handle_text_input(event.text.text)) {
        render_.set_chrome_dirty();
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
                render_.set_all_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::W) {
            if (app_.close_active_tab()) {
                render_.set_all_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::Right) {
            if (app_.activate_next_tab()) {
                render_.set_all_dirty();
            }
            return true;
        }

        if (event.mods.ctrl && !event.mods.shift && event.key.key == Key::Left) {
            if (app_.activate_prev_tab()) {
                render_.set_all_dirty();
            }
            return true;
        }

        // Alt+Left / Alt+Right: back / forward over the tab's history (7.6.1).
        if (event.mods.alt && !event.mods.ctrl && event.key.key == Key::Left) {
            app_.navigate_back();
            return true;
        }

        if (event.mods.alt && !event.mods.ctrl && event.key.key == Key::Right) {
            app_.navigate_forward();
            return true;
        }
    }
    return false;
}

bool ChromeEventRouter::handle_global_key_shortcut(const Hummingbird::InputEvent& event) {
    if (event.key.key == Key::L && event.mods.ctrl && event.mods.shift) {
        HB_LOG_INFO("[ui] Forcing document repaint");
        render_.set_document_dirty();
        return true;
    }

    if (event.key.key == Key::L && event.mods.ctrl) {
        chrome_.url_bar().set_active(true, window_, "[ui] URL bar focused");
        chrome_.url_bar().move_caret_to_end();
        if (app_.active_tab().clear_input_focus()) {
            render_.set_controls_dirty();
        }
        render_.set_chrome_dirty();
        return true;
    }

    // Ctrl+D: bookmark the current page (7.6.2).
    if (event.key.key == Key::D && event.mods.ctrl && !event.key.repeat) {
        app_.bookmark_active_tab();
        return true;
    }

    // Ctrl+Shift+O: open the bookmarks page (matches Edge/Firefox).
    if (event.key.key == Key::O && event.mods.ctrl && event.mods.shift && !event.key.repeat) {
        app_.navigate_and_reflect_url("about:bookmarks");
        return true;
    }

    // F5 / Ctrl+R: reload. Ctrl+Shift+R / Ctrl+F5: hard reload. Reloading rather
    // than re-navigating to requested_url() so an edit the user typed but did not
    // submit does not become the target, AND so the HTTP cache revalidates
    // instead of serving (9.3.1) — a plain re-navigation would hand back the same
    // cached bytes and make F5 look broken.
    //
    // The two levels are the browser convention and they differ in reach: a normal
    // reload re-checks the document, a hard reload ignores the cache for its
    // subresources too. That distinction is the answer to "I changed the
    // stylesheet and nothing happened".
    if ((event.key.key == Key::F5 || (event.key.key == Key::R && event.mods.ctrl)) && !event.key.repeat) {
        const bool hard = event.mods.shift || (event.key.key == Key::F5 && event.mods.ctrl);
        app_.reload_and_reflect_url(hard);
        return true;
    }

    // Ctrl+Shift+U: flip the current site between honest (Transparent) and
    // Chrome-shaped (Compatibility) identity and reload. The opt-in escape hatch
    // for sites that reject Hummingbird's real User-Agent (e.g. HN's 429).
    if (event.key.key == Key::U && event.mods.ctrl && event.mods.shift && !event.key.repeat) {
        app_.toggle_active_site_compatibility();
        return true;
    }

    if (event.key.key == Key::F1) {
        render_.toggle_debug_outlines();
        HB_LOG_INFO("[ui] Debug outlines " << (render_.debug_outlines() ? "ON" : "OFF"));
        render_.set_document_dirty();
        return true;
    }
    return false;
}

bool ChromeEventRouter::handle_url_bar_key_down(const Hummingbird::InputEvent& event) {
    auto result = chrome_.url_bar().handle_key_down(event, window_);
    if (!result.handled) {
        return false;
    }
    if (result.submitted_url) {
        app_.navigate_and_reflect_url(*result.submitted_url);
    }
    if (result.needs_repaint) {
        render_.set_chrome_dirty();
    }
    return true;
}

bool ChromeEventRouter::handle_tab_strip_mouse_down(const Hummingbird::InputEvent& event) {
    if (!graphics_ || !window_) {
        return false;
    }
    const int tabs_top_y = chrome_.url_bar().height();
    const int tabs_bottom_y = chrome_.url_bar().height() + chrome_.tab_strip_height();
    if (event.mouse_button.y < tabs_top_y || event.mouse_button.y >= tabs_bottom_y) {
        return false;
    }
    const auto [win_w, win_h] = window_->get_size();
    (void)win_h;
    auto result = chrome_.handle_tab_strip_mouse_down(event.mouse_button.x, event.mouse_button.y, win_w, tabs_top_y,
                                                      tabs_.manager());
    if (!result.handled) {
        return false;
    }
    if (result.activated_tab) {
        (void)app_.activate_tab(*result.activated_tab);
    }
    render_.set_all_dirty();
    return true;
}

bool ChromeEventRouter::handle_url_bar_mouse_down(const Hummingbird::InputEvent& event) {
    auto url_result = chrome_.url_bar().handle_mouse_down(event.mouse_button.x, event.mouse_button.y, window_);
    if (!url_result.handled) {
        return false;
    }

    auto& tab = app_.active_tab();
    bool interaction_state_changed = false;
    interaction_state_changed |= tab.clear_control_interaction();
    interaction_state_changed |= tab.clear_input_focus();
    if (interaction_state_changed && graphics_ && window_) {
        auto [w, h] = window_->get_size();
        const auto viewport = chrome_.content_viewport(w, h);
        if (tab.refresh_styles_for_interaction(*graphics_, viewport)) {
            render_.set_document_and_controls_dirty();
        }
    } else if (interaction_state_changed) {
        render_.set_document_and_controls_dirty();
    }
    if (url_result.security_override_requested) {
        if (app_.active_tab().allow_insecure_for_current_host()) {
            app_.navigate_and_reflect_url(app_.active_tab().requested_url());
        }
    }
    app_.set_tab_text_input_active(false);
    if (url_result.needs_repaint) {
        render_.set_chrome_dirty();
    }
    return true;
}

}  // namespace Hummingbird::App
