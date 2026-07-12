#include "app/BrowserChrome.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/utils/Log.h"

namespace Hummingbird::App {

namespace {
std::optional<ImageBitmap> load_icon(IResourceProvider* provider, IImageDecoder* decoder, std::string_view path) {
    if (!provider || !decoder) return std::nullopt;
    auto bytes = provider->load_bytes(path);
    if (!bytes) return std::nullopt;
    auto decoded = decoder->decode(*bytes);
    if (!decoded) {
        HB_LOG_WARN("[ui] failed to decode icon: " << path);
    }
    return decoded;
}
}  // namespace

Layout::Rect BrowserChrome::content_viewport(int win_w, int win_h) const {
    const int content_y = url_bar_.height() + tab_strip_height();
    const int content_h = std::max(0, win_h - content_y);
    return {0.0f, static_cast<float>(content_y), static_cast<float>(win_w), static_cast<float>(content_h)};
}

void BrowserChrome::draw_tab_strip(IGraphicsContext& graphics, int win_w, int top_y,
                                   const Engine::TabManager& tabs) const {
    tab_strip_.draw(graphics, win_w, top_y, tabs);
}

BrowserChrome::TabStripClickResult BrowserChrome::handle_tab_strip_mouse_down(int x, int y, int win_w, int top_y,
                                                                              const Engine::TabManager& tabs) const {
    auto result = tab_strip_.handle_mouse_down(x, y, win_w, top_y, tabs);
    return {result.handled, result.needs_repaint, result.activated_tab};
}

void BrowserChrome::load_security_icons(IResourceProvider* provider, IImageDecoder* decoder) {
    UrlBar::SecurityIcons icons;
    icons.secure = load_icon(provider, decoder, "assets/icons/page_security/secure.png");
    icons.insecure = load_icon(provider, decoder, "assets/icons/page_security/insecure.png");
    icons.asecure = load_icon(provider, decoder, "assets/icons/page_security/asecure.png");
    url_bar_.set_security_icons(std::move(icons));
}

}  // namespace Hummingbird::App
