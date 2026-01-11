#include "app/UrlBar.h"

#include <algorithm>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "layout/Geometry.h"

namespace {
constexpr Color kOverlayBg{220, 220, 220, 255};
constexpr Color kOverlayText{0, 0, 0, 255};
}  // namespace

UrlBar::UrlBar() : text_("https://example.dev") {
    text_.reserve(2048);
    render_text_.reserve(2049);

    font_path_ = Hummingbird::resolve_asset_path_string("assets/fonts/Roboto-Regular.ttf");
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

bool UrlBar::handle_mouse_down(int x, int y, IWindow* window) {
    if (y >= height_) return false;
    set_active(true, window, "[ui] URL bar focused (mouse)");
    return true;
}

void UrlBar::draw(IGraphicsContext& graphics, int win_w) const {
    Hummingbird::Layout::Rect bar{0, 0, static_cast<float>(win_w), static_cast<float>(height_)};
    graphics.fill_rect(bar, kOverlayBg);
    graphics.draw_text(render_text_, 8.0f, 8.0f, style_);
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
