#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"

namespace Hummingbird::App {

class UrlBar {
public:
    UrlBar();

    struct KeyResult {
        bool handled = false;
        bool needs_repaint = false;
        std::optional<std::string> submitted_url;
    };

    const std::string& text() const { return text_; }
    bool is_active() const { return active_; }
    int height() const { return height_; }

    void set_text(std::string_view text);
    void set_active(bool active, IWindow* window, const char* log_message);
    void move_caret_to_end();

    bool handle_text_input(std::string_view text);
    KeyResult handle_key_down(const InputEvent& event, IWindow* window);
    bool handle_mouse_down(int x, int y, IWindow* window);

    void draw(IGraphicsContext& graphics, int win_w) const;

private:
    void refresh_render_text();
    void insert_text(std::string_view text);

    static std::string::size_type clamp_caret(std::string::size_type caret, std::string_view text);
    static std::string::size_type prev_codepoint(std::string_view text, std::string::size_type caret);
    static std::string::size_type next_codepoint(std::string_view text, std::string::size_type caret);

    std::string text_;
    std::string render_text_;
    std::string::size_type caret_ = 0;
    std::string font_path_;
    TextStyle style_;
    bool active_ = true;
    int height_ = 32;
};

}  // namespace Hummingbird::App
