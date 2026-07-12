#include "app/TabStrip.h"

#include <algorithm>

#include "core/utils/AssetPath.h"
#include "engine/tab/Tab.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::App {

namespace {
constexpr Color kStripBg{200, 200, 200, 255};
constexpr Color kTabActiveBg{240, 240, 240, 255};
constexpr Color kTabInactiveBg{180, 180, 180, 255};
constexpr Color kTabText{0, 0, 0, 255};

constexpr float kTextSize = 13.0f;
constexpr float kTextPaddingX = 8.0f;
constexpr float kTextBaselineY = 6.0f;

constexpr int kTabWidth = 160;
}  // namespace

TabStrip::TabStrip() {
    style_.font_path = Hummingbird::Core::Utils::resolve_asset_path_string("assets/fonts/Roboto-Regular.ttf");
    style_.font_size = kTextSize;
    style_.color = kTabText;
}

void TabStrip::draw(IGraphicsContext& graphics, int win_w, int top_y, const Engine::TabManager& tabs) const {
    Hummingbird::Layout::Rect strip{0, static_cast<float>(top_y), static_cast<float>(win_w),
                                    static_cast<float>(height_)};
    graphics.fill_rect(strip, kStripBg);

    const auto ids = tabs.tab_ids();
    const auto active = tabs.active_tab_id();
    const size_t max_tabs = static_cast<size_t>(std::max(0, win_w / kTabWidth));

    const size_t count = std::min(ids.size(), max_tabs);
    for (size_t i = 0; i < count; ++i) {
        const int x = static_cast<int>(i) * kTabWidth;
        Hummingbird::Layout::Rect tab_rect{static_cast<float>(x), static_cast<float>(top_y),
                                           static_cast<float>(kTabWidth), static_cast<float>(height_)};
        const bool is_active = active && *active == ids[i];
        graphics.fill_rect(tab_rect, is_active ? kTabActiveBg : kTabInactiveBg);

        graphics.draw_text(label_for_tab(tabs, ids[i], static_cast<int>(i)), static_cast<float>(x) + kTextPaddingX,
                           static_cast<float>(top_y) + kTextBaselineY, style_);
    }
}

TabStrip::MouseResult TabStrip::handle_mouse_down(int x, int y, int win_w, int top_y,
                                                  const Engine::TabManager& tabs) const {
    MouseResult result;
    if (y < top_y || y >= top_y + height_) return result;

    const auto ids = tabs.tab_ids();
    if (ids.empty()) return result;

    const int index = x / kTabWidth;
    if (index < 0) return result;

    const int max_tabs = std::max(0, win_w / kTabWidth);
    if (index >= max_tabs) return result;
    if (static_cast<size_t>(index) >= ids.size()) return result;

    result.handled = true;
    result.needs_repaint = true;
    result.activated_tab = ids[static_cast<size_t>(index)];
    return result;
}

std::string TabStrip::label_for_tab(const Engine::TabManager& tabs, Engine::TabId id, int ordinal) const {
    const Engine::Tab* tab = tabs.tab_by_id(id);
    std::string label = std::to_string(ordinal + 1);
    label.append(": ");
    if (!tab) {
        label.append("Tab");
        return label;
    }

    std::string_view url = tab->requested_url();
    if (url.empty()) {
        label.append("New Tab");
        return label;
    }

    constexpr size_t kMaxLen = 24;
    if (url.size() <= kMaxLen) {
        label.append(url);
        return label;
    }
    label.append(url.substr(0, kMaxLen));
    label.append("...");
    return label;
}

}  // namespace Hummingbird::App
