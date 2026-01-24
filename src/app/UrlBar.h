#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/SecurityState.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/InputEvent.h"

namespace Hummingbird {
class IWindow;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::App {

class UrlBar {
public:
    UrlBar();

    struct SecurityIcons {
        std::optional<ImageBitmap> secure;
        std::optional<ImageBitmap> insecure;
        std::optional<ImageBitmap> asecure;
    };

    struct KeyResult {
        bool handled = false;
        bool needs_repaint = false;
        std::optional<std::string> submitted_url;
    };

    struct MouseResult {
        bool handled = false;
        bool needs_repaint = false;
        bool security_override_requested = false;
    };

    const std::string& text() const { return text_; }
    bool is_active() const { return active_; }
    int height() const { return height_; }
    SecurityState security_state() const { return security_state_; }

    void set_text(std::string_view text);
    void set_active(bool active, IWindow* window, const char* log_message);
    void move_caret_to_end();
    void set_security_icons(SecurityIcons icons);
    bool set_security_state(SecurityState state);

    bool handle_text_input(std::string_view text);
    KeyResult handle_key_down(const InputEvent& event, IWindow* window);
    MouseResult handle_mouse_down(int x, int y, IWindow* window);

    void draw(IGraphicsContext& graphics, int win_w) const;

private:
    void refresh_render_text();
    void insert_text(std::string_view text);
    const ImageBitmap* current_icon() const;
    float text_start_x() const;
    bool is_security_icon_hit(int x, int y) const;

    std::string text_;
    std::string render_text_;
    std::string::size_type caret_ = 0;
    std::string font_path_;
    TextStyle style_;
    bool active_ = true;
    int height_ = 32;
    SecurityState security_state_ = SecurityState::Unknown;
    SecurityIcons security_icons_{};
};

}  // namespace Hummingbird::App
