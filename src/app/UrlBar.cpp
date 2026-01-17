#include "app/UrlBar.h"

#include <algorithm>

#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "layout/Geometry.h"

namespace Hummingbird::App {

namespace {
constexpr Color kOverlayBg{220, 220, 220, 255};
constexpr Color kOverlayText{0, 0, 0, 255};
constexpr float kIconSize = 16.0f;
constexpr float kIconPadding = 6.0f;
constexpr float kTextPadding = 8.0f;
}  // namespace

UrlBar::UrlBar() : text_("https://example.dev") {
    text_.reserve(2048);
    render_text_.reserve(2049);

    font_path_ = Hummingbird::Core::Utils::resolve_asset_path_string("assets/fonts/Roboto-Regular.ttf");
    style_.font_path = font_path_;
    style_.font_size = 16.0f;
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

    if (!event.key.repeat) {
        const bool paste_ctrl_v = event.mods.ctrl && event.key.key == Key::V;
        const bool paste_shift_insert = event.mods.shift && event.key.key == Key::Insert;
        if (paste_ctrl_v || paste_shift_insert) {
            if (window) {
                const auto clipboard_text = window->get_clipboard_text();
                if (!clipboard_text.empty()) {
                    insert_text(clipboard_text);
                    result.handled = true;
                    result.needs_repaint = true;
                }
            }
            return result;
        }
    }

    if (event.key.key == Key::Backspace) {
        result.handled = true;
        if (!text_.empty()) {
            caret_ = clamp_caret(caret_, text_);
            if (caret_ > 0) {
                auto start = prev_codepoint(text_, caret_);
                text_.erase(start, caret_ - start);
                caret_ = start;
            }
            refresh_render_text();
        }
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Delete) {
        result.handled = true;
        if (!text_.empty()) {
            caret_ = clamp_caret(caret_, text_);
            if (caret_ < text_.size()) {
                auto end = next_codepoint(text_, caret_);
                text_.erase(caret_, end - caret_);
            }
            refresh_render_text();
        }
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Left) {
        caret_ = prev_codepoint(text_, caret_);
        refresh_render_text();
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Right) {
        caret_ = next_codepoint(text_, caret_);
        refresh_render_text();
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Home) {
        caret_ = 0;
        refresh_render_text();
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::End) {
        caret_ = text_.size();
        refresh_render_text();
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Enter) {
        set_active(false, window, nullptr);
        result.submitted_url = text_;
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Escape) {
        set_active(false, window, nullptr);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

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
    graphics.draw_text(render_text_, text_start_x(), 8.0f, style_);
}

void UrlBar::refresh_render_text() {
    render_text_.assign(text_);
    if (active_) {
        caret_ = clamp_caret(caret_, text_);
        render_text_.insert(caret_, "|");
    }
}

void UrlBar::insert_text(std::string_view text) {
    if (text.empty()) return;
    caret_ = clamp_caret(caret_, text_);
    text_.insert(caret_, text);
    caret_ += text.size();
    refresh_render_text();
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

std::string::size_type UrlBar::clamp_caret(std::string::size_type caret, std::string_view text) {
    return std::min(caret, text.size());
}

std::string::size_type UrlBar::prev_codepoint(std::string_view text, std::string::size_type caret) {
    caret = clamp_caret(caret, text);
    if (caret == 0) return 0;
    std::string::size_type i = caret - 1;
    while (i > 0 && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
        --i;
    }
    return i;
}

std::string::size_type UrlBar::next_codepoint(std::string_view text, std::string::size_type caret) {
    caret = clamp_caret(caret, text);
    if (caret >= text.size()) return text.size();
    std::string::size_type i = caret + 1;
    while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
        ++i;
    }
    return i;
}

}  // namespace Hummingbird::App
