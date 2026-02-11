#include "app/RenderCoordinator.h"

#include <cctype>
#include <cstdlib>
#include <string_view>

#include "app/BrowserApp.h"
#include "core/utils/Log.h"

namespace Hummingbird::App {

namespace {
constexpr Color kClearColor{255, 255, 255, 255};

bool env_truthy(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !value[0]) return false;
    std::string_view view(value);
    auto equals = [&](std::string_view needle) {
        if (view.size() != needle.size()) return false;
        for (size_t i = 0; i < view.size(); ++i) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(view[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i])));
            if (a != b) return false;
        }
        return true;
    };
    return equals("1") || equals("true") || equals("yes") || equals("on");
}

bool render_loop_debug_enabled() {
    return env_truthy("HB_DEBUG_RENDER_LOOP");
}
}  // namespace

void RenderCoordinator::render_if_needed() {
    if (!app_.graphics_ || (!document_dirty_ && !chrome_dirty_ && !controls_dirty_)) return;

    if (render_loop_debug_enabled()) {
        static int render_log_count = 0;
        ++render_log_count;
        if (render_log_count <= 30 || (render_log_count % 120) == 0) {
            HB_LOG_WARN("[render-debug] frame count=" << render_log_count << " document_dirty=" << document_dirty_
                                                      << " chrome_dirty=" << chrome_dirty_ << " controls_dirty="
                                                      << controls_dirty_ << " cache_valid=" << document_cache_valid_);
        }
    }

    auto [win_w, win_h] = app_.window_->get_size();

    Hummingbird::Layout::Rect full{0, 0, static_cast<float>(win_w), static_cast<float>(win_h)};
    app_.graphics_->set_viewport(full);
    const auto viewport = app_.compute_content_viewport(win_w, win_h);
    if (!document_cache_valid_) {
        document_dirty_ = true;
    }
    if (document_dirty_) {
        document_cache_valid_ = false;
        if (app_.graphics_->begin_document_cache(full)) {
            app_.active_tab().paint(*app_.graphics_, viewport, app_.debug_outlines_);
            app_.graphics_->end_document_cache();
            document_cache_valid_ = true;
        } else {
            app_.graphics_->clear(kClearColor);
            app_.graphics_->set_text_cache_owner(0);
            app_.browser_chrome_.url_bar().draw(*app_.graphics_, win_w);
            app_.browser_chrome_.draw_tab_strip(*app_.graphics_, win_w, app_.browser_chrome_.url_bar().height(),
                                               app_.tab_controller_.manager());
            app_.active_tab().paint(*app_.graphics_, viewport, app_.debug_outlines_);
            app_.graphics_->present();
            document_dirty_ = false;
            chrome_dirty_ = false;
            controls_dirty_ = false;
            return;
        }
    }

    app_.graphics_->clear(kClearColor);
    if (document_cache_valid_) {
        app_.graphics_->draw_document_cache();
    }
    app_.graphics_->set_viewport(full);
    app_.graphics_->set_text_cache_owner(0);
    app_.browser_chrome_.url_bar().draw(*app_.graphics_, win_w);
    app_.browser_chrome_.draw_tab_strip(*app_.graphics_, win_w, app_.browser_chrome_.url_bar().height(),
                                       app_.tab_controller_.manager());
    if (!document_dirty_ && controls_dirty_) {
        app_.active_tab().paint_controls(*app_.graphics_, viewport);
    }

    app_.graphics_->present();
    document_dirty_ = false;
    chrome_dirty_ = false;
    controls_dirty_ = false;
}

}  // namespace Hummingbird::App
