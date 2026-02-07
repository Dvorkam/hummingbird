#include "app/UrlBar.h"

#include <algorithm>

#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "core/utils/TextEditBuffer.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::App {

namespace {
constexpr Color kOverlayBg{220, 220, 220, 255};
constexpr Color kOverlayText{0, 0, 0, 255};
constexpr float kIconSize = 16.0f;
constexpr float kIconPadding = 6.0f;
constexpr float kTextPadding = 8.0f;
constexpr float kTextBaselineY = 8.0f;
constexpr float kTextSize = 16.0f;
}  // namespace

UrlBar::UrlBar() : text_("https://example.dev") {
    text_.reserve(2048);
    render_text_.reserve(2049);

    font_path_ = Hummingbird::Core::Utils::resolve_asset_path_string("assets/fonts/Roboto-Regular.ttf");
    style_.font_path = font_path_;
    style_.font_size = kTextSize;
    style_.color = kOverlayText;

    refresh_render_text();
}

void UrlBar::set_text(std::string_view text) {
    text_.assign(text);
    caret_ = text_.size();
    refresh_render_text();
}

void UrlBar::set_active(bool active, IWindow* window, const char* log_message) {
    active_ = active;
    if (active_) {
        caret_ = text_.size();
    }
    if (window) {
        if (active) {
            window->start_text_input();
        } else {
            window->stop_text_input();
        }
    }
    refresh_render_text();
    if (log_message) {
        HB_LOG_INFO(log_message);
    }
}

void UrlBar::move_caret_to_end() {
    caret_ = text_.size();
    refresh_render_text();
}

void UrlBar::set_security_icons(SecurityIcons icons) {
    security_icons_ = std::move(icons);
}

bool UrlBar::set_security_state(SecurityState state) {
    if (security_state_ == state) return false;
    security_state_ = state;
    return true;
}

bool UrlBar::handle_text_input(std::string_view text) {
    if (!active_ || text.empty()) return false;
    insert_text(text);
    return true;
}

UrlBar::KeyResult UrlBar::handle_key_down(const InputEvent& event, IWindow* window) {
    KeyResult result;
    if (!active_) return result;

    if (handle_paste_key(event, window, result)) return result;
    if (handle_edit_key(event, result)) return result;
    if (handle_commit_key(event, window, result)) return result;

    return result;
}

UrlBar::MouseResult UrlBar::handle_mouse_down(int x, int y, IWindow* window) {
    MouseResult result;
    if (y >= height_) return result;

    result.handled = true;
    result.needs_repaint = true;

    if (security_state_ == SecurityState::InsecureTls && is_security_icon_hit(x, y)) {
        result.security_override_requested = true;
    }

    set_active(true, window, "[ui] URL bar focused (mouse)");
    return result;
}

void UrlBar::draw(IGraphicsContext& graphics, int win_w) const {
    Hummingbird::Layout::Rect bar{0, 0, static_cast<float>(win_w), static_cast<float>(height_)};
    graphics.fill_rect(bar, kOverlayBg);
    const ImageBitmap* icon = current_icon();
    if (icon) {
        const float icon_y = (static_cast<float>(height_) - kIconSize) * 0.5f;
        Hummingbird::Layout::Rect icon_rect{kTextPadding, icon_y, kIconSize, kIconSize};
        graphics.draw_image(*icon, icon_rect);
    }
    graphics.draw_text(render_text_, text_start_x(), kTextBaselineY, style_);
}

void UrlBar::refresh_render_text() {
    render_text_.assign(text_);
    if (active_) {
        caret_ = Core::Utils::TextEditBuffer::clamp_caret_for(text_, caret_);
        render_text_.insert(caret_, "|");
    }
}

void UrlBar::insert_text(std::string_view text) {
    if (Core::Utils::TextEditBuffer::insert_text(text_, caret_, text)) {
        refresh_render_text();
    }
}

bool UrlBar::handle_paste_key(const InputEvent& event, IWindow* window, KeyResult& result) {
    if (event.key.repeat) return false;

    const bool paste_ctrl_v = event.mods.ctrl && event.key.key == Key::V;
    const bool paste_shift_insert = event.mods.shift && event.key.key == Key::Insert;
    if (!paste_ctrl_v && !paste_shift_insert) return false;

    if (window) {
        const auto clipboard_text = window->get_clipboard_text();
        if (!clipboard_text.empty()) {
            insert_text(clipboard_text);
            result.handled = true;
            result.needs_repaint = true;
        }
    }
    return true;
}

bool UrlBar::handle_edit_key(const InputEvent& event, KeyResult& result) {
    if (event.key.key == Key::Backspace) {
        result.handled = true;
        if (Core::Utils::TextEditBuffer::backspace(text_, caret_)) {
            refresh_render_text();
        }
        result.needs_repaint = true;
        return true;
    }

    if (event.key.key == Key::Delete) {
        result.handled = true;
        if (Core::Utils::TextEditBuffer::delete_forward(text_, caret_)) {
            refresh_render_text();
        }
        result.needs_repaint = true;
        return true;
    }

    if (event.key.key == Key::Left) {
        Core::Utils::TextEditBuffer::move_left(text_, caret_);
        refresh_render_text();
        result.handled = true;
        result.needs_repaint = true;
        return true;
    }

    if (event.key.key == Key::Right) {
        Core::Utils::TextEditBuffer::move_right(text_, caret_);
        refresh_render_text();
        result.handled = true;
        result.needs_repaint = true;
        return true;
    }

    if (event.key.key == Key::Home) {
        Core::Utils::TextEditBuffer::move_home(caret_);
        refresh_render_text();
        result.handled = true;
        result.needs_repaint = true;
        return true;
    }

    if (event.key.key == Key::End) {
        Core::Utils::TextEditBuffer::move_end(text_, caret_);
        refresh_render_text();
        result.handled = true;
        result.needs_repaint = true;
        return true;
    }

    return false;
}

bool UrlBar::handle_commit_key(const InputEvent& event, IWindow* window, KeyResult& result) {
    if (event.key.key == Key::Enter) {
        set_active(false, window, nullptr);
        result.submitted_url = text_;
        result.handled = true;
        result.needs_repaint = true;
        return true;
    }

    if (event.key.key == Key::Escape) {
        set_active(false, window, nullptr);
        result.handled = true;
        result.needs_repaint = true;
        return true;
    }

    return false;
}

const ImageBitmap* UrlBar::current_icon() const {
    switch (security_state_) {
        case SecurityState::Secure:
            return security_icons_.secure ? &*security_icons_.secure : nullptr;
        case SecurityState::InsecureTls:
            return security_icons_.insecure ? &*security_icons_.insecure : nullptr;
        case SecurityState::InsecureHttp:
            return security_icons_.asecure ? &*security_icons_.asecure : nullptr;
        case SecurityState::Unknown:
        default:
            return nullptr;
    }
}

float UrlBar::text_start_x() const {
    return current_icon() ? (kTextPadding + kIconSize + kIconPadding) : kTextPadding;
}

bool UrlBar::is_security_icon_hit(int x, int y) const {
    if (!current_icon()) return false;
    const float icon_y = (static_cast<float>(height_) - kIconSize) * 0.5f;
    return x >= static_cast<int>(kTextPadding) && x <= static_cast<int>(kTextPadding + kIconSize) &&
           y >= static_cast<int>(icon_y) && y <= static_cast<int>(icon_y + kIconSize);
}

}  // namespace Hummingbird::App
