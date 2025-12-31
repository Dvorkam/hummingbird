#include "app/BrowserApp.h"

#include <algorithm>
#include <utility>

#include "core/platform_api/NetworkFactory.h"
#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "html/HtmlParser.h"
#include "style/CssParser.h"

// Include concrete definitions:
#include "core/dom/Node.h"
#include "layout/RenderObject.h"

namespace {
// You can keep constants here to avoid re-allocating per frame
constexpr Color kClearColor{255, 255, 255, 255};
constexpr Color kOverlayBg{220, 220, 220, 255};
constexpr Color kOverlayText{0, 0, 0, 255};

size_t count_nodes_recursive(const Hummingbird::DOM::Node* node) {
    if (!node) return 0;
    size_t total = 1;
    for (const auto& child : node->get_children()) {
        total += count_nodes_recursive(child.get());
    }
    return total;
}
}  // namespace

BrowserApp::BrowserApp(std::unique_ptr<IWindow> window) : window_(std::move(window)) {
    graphics_ = window_ ? window_->get_graphics_context() : nullptr;
    network_ = create_network(NetworkBackend::Curl);
    fallback_network_ = create_network(NetworkBackend::Stub);
    if (!network_ || !fallback_network_) {
        HB_LOG_ERROR("[network] failed to create network backend(s)");
    }
}

BrowserApp::~BrowserApp() {
    shutdown();
}

void BrowserApp::shutdown() {
    // run once
    if (shutting_down_.exchange(true, std::memory_order_relaxed)) return;

    // stop input first (safe even if already stopped)
    if (window_) window_->stop_text_input();

    // invalidate nav id so late responses become no-ops (while we're still alive)
    active_nav_.store(UINT64_MAX, std::memory_order_relaxed);

    // stop/join async work BEFORE pending_mutex_ etc can die
    if (network_) network_->shutdown();
    if (fallback_network_) fallback_network_->shutdown();

    // optional: clear pending html
    {
        std::lock_guard<std::mutex> lg(pending_mutex_);
        pending_html_.reset();
    }

    // close window last (or earlier if you prefer to hide UI immediately)
    if (window_ && window_->is_open()) window_->close();
}

void BrowserApp::start() {
    // initial navigation
    load_url(url_bar_text_);

    // initial focus
    url_bar_active_ = true;
    window_->start_text_input();
    HB_LOG_INFO("[ui] URL bar focused (default)");
    HB_LOG_INFO("[ui] Starting app. Press Ctrl+L to focus URL bar.");
}

bool BrowserApp::tick() {
    if (!window_ || !window_->is_open()) return false;

    window_->update();

    pump_events();

    if (!window_->is_open()) return false;

    consume_pending_html_and_rebuild();
    render_if_needed();

    return window_->is_open();
}

void BrowserApp::pump_events() {
    InputEvent e;
    if (window_->wait_event(e, wait_timeout_ms_)) {
        handle_event(e);
    }

    int processed = 0;
    while (processed++ < max_events_per_tick_ && window_->poll_event(e)) {
        handle_event(e);
        if (!window_->is_open()) break;
    }
}

void BrowserApp::handle_event(const InputEvent& event) {
    switch (event.type) {
        case EventType::Quit:
            handle_quit_event();
            return;
        case EventType::TextInput:
            handle_text_input_event(event);
            return;
        case EventType::KeyDown:
            handle_key_down_event(event);
            return;
        case EventType::MouseDown:
            handle_mouse_down_event(event);
            return;
        case EventType::MouseWheel:
            handle_mouse_wheel_event(event);
            return;
        case EventType::Resize:
            handle_resize_event(event);
            return;
        default:
            return;
    }
}

void BrowserApp::handle_quit_event() {
    shutdown();
}

void BrowserApp::handle_text_input_event(const InputEvent& event) {
    if (!url_bar_active_) return;
    url_bar_text_ += event.text.text;
    needs_repaint_ = true;
}

void BrowserApp::handle_key_down_event(const InputEvent& event) {
    if (event.key.key == Key::Backspace && url_bar_active_ && !url_bar_text_.empty()) {
        url_bar_text_.pop_back();
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Enter) {
        set_url_bar_active(false, nullptr);
        load_url(url_bar_text_);
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::Escape) {
        set_url_bar_active(false, nullptr);
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::F1) {
        debug_outlines_ = !debug_outlines_;
        HB_LOG_INFO("[ui] Debug outlines " << (debug_outlines_ ? "ON" : "OFF"));
        needs_repaint_ = true;
        return;
    }

    if (event.key.key == Key::L && event.mods.ctrl) {
        set_url_bar_active(true, "[ui] URL bar focused");
        needs_repaint_ = true;
        return;
    }

    needs_repaint_ = true;
}

void BrowserApp::handle_mouse_down_event(const InputEvent& event) {
    const int y = event.mouse_button.y;
    if (y < url_bar_height_) {
        set_url_bar_active(true, "[ui] URL bar focused (mouse)");
    } else {
        set_url_bar_active(false, nullptr);
    }
    needs_repaint_ = true;
}

void BrowserApp::handle_mouse_wheel_event(const InputEvent& event) {
    const float delta = static_cast<float>(event.wheel.dy) * 32.0f;
    scroll_y_ -= delta;

    auto [win_w, win_h] = window_->get_size();
    const float viewport_h = static_cast<float>(win_h - url_bar_height_);
    clamp_scroll(viewport_h);

    needs_repaint_ = true;
}

void BrowserApp::handle_resize_event(const InputEvent& event) {
    relayout_for_window(event.resize.width, event.resize.height);
    needs_repaint_ = true;
}

void BrowserApp::set_url_bar_active(bool active, const char* log_message) {
    url_bar_active_ = active;
    if (window_) {
        if (active) {
            window_->start_text_input();
        } else {
            window_->stop_text_input();
        }
    }
    if (log_message) {
        HB_LOG_INFO(log_message);
    }
}

void BrowserApp::clamp_scroll(float viewport_height) {
    const float max_scroll = std::max(0.0f, content_height_ - viewport_height);
    scroll_y_ = std::clamp(scroll_y_, 0.0f, max_scroll);
}

void BrowserApp::relayout_for_window(int win_w, int win_h) {
    if (!render_tree_ || !graphics_) return;

    const auto layout_start = Hummingbird::Core::Clock::now();
    const int content_h = std::max(0, win_h - url_bar_height_);
    Hummingbird::Layout::Rect viewport{0.0f, static_cast<float>(url_bar_height_), static_cast<float>(win_w),
                                       static_cast<float>(content_h)};

    render_tree_->layout(*graphics_, viewport);
    const auto layout_end = Hummingbird::Core::Clock::now();
    content_height_ = render_tree_->get_rect().height;
    clamp_scroll(viewport.height);
    HB_LOG_INFO("[perf] layout ms=" << Hummingbird::Core::duration_ms(layout_start, layout_end)
                                    << " viewport=" << viewport.width << "x" << viewport.height);
}

void BrowserApp::load_url(const std::string& url) {
    const uint64_t id = ++nav_counter_;
    active_nav_.store(id, std::memory_order_relaxed);

    requested_url_ = url;

    if (!network_) {
        HB_LOG_ERROR("[network] no backend available for " << url);
        if (fallback_network_) {
            fallback_network_->get(url, [this, id](std::string body) {
                if (id != active_nav_.load(std::memory_order_relaxed)) return;
                std::lock_guard<std::mutex> lg(pending_mutex_);
                pending_html_ = std::move(body);
            });
        }
        return;
    }

    network_->get(url, [this, id, url](std::string body) {
        if (id != active_nav_.load(std::memory_order_relaxed)) return;

        if (body.empty()) {
            HB_LOG_WARN("[network] curl returned empty for " << url << ", using stub");

            if (!fallback_network_) return;
            fallback_network_->get(url, [this, id](std::string fallback) {
                if (id != active_nav_.load(std::memory_order_relaxed)) return;
                std::lock_guard<std::mutex> lg(pending_mutex_);
                pending_html_ = std::move(fallback);
            });
            return;
        }

        HB_LOG_INFO("[network] fetched " << body.size() << " bytes from " << url);
        std::lock_guard<std::mutex> lg(pending_mutex_);
        pending_html_ = std::move(body);
    });
}

void BrowserApp::consume_pending_html_and_rebuild() {
    auto html = take_pending_html();
    if (!html) return;
    rebuild_from_html(*html);
}

std::optional<std::string> BrowserApp::take_pending_html() {
    std::optional<std::string> html;
    std::lock_guard<std::mutex> lg(pending_mutex_);
    if (pending_html_) {
        html = std::move(pending_html_);
        pending_html_.reset();
    }
    return html;
}

void BrowserApp::rebuild_from_html(const std::string& html) {
    reset_document_state();
    HB_LOG_INFO("[pipeline] html size: " << html.size());

    std::vector<std::string> style_blocks;
    if (!parse_html(html, style_blocks)) {
        return;
    }

    std::string css = build_css_source(style_blocks);
    parse_and_apply_css(css);

    if (!build_render_tree()) {
        return;
    }

    layout_current_window();
    HB_LOG_INFO("[pipeline] render tree root children: " << render_tree_->get_children().size());
    needs_repaint_ = true;
}

void BrowserApp::reset_document_state() {
    dom_tree_.reset();
    render_tree_.reset();
    dom_arena_.reset();
}

bool BrowserApp::parse_html(const std::string& html, std::vector<std::string>& style_blocks) {
    const auto parse_start = Hummingbird::Core::Clock::now();
    Hummingbird::Html::Parser parser(dom_arena_, html);
    auto parse_result = parser.parse();
    const auto parse_end = Hummingbird::Core::Clock::now();

    dom_tree_ = std::move(parse_result.dom);
    style_blocks = std::move(parse_result.style_blocks);

    if (!dom_tree_) {
        HB_LOG_WARN("[pipeline] parsed empty DOM");
        return false;
    }

    HB_LOG_INFO("[pipeline] parsed DOM children: " << dom_tree_->get_children().size()
                                                   << " total nodes: " << count_nodes_recursive(dom_tree_.get()));
    HB_LOG_INFO("[perf] html parse ms=" << Hummingbird::Core::duration_ms(parse_start, parse_end));
    return true;
}

std::string BrowserApp::build_css_source(const std::vector<std::string>& style_blocks) const {
    std::string css = "body { padding: 8px; } p { margin: 4px; }";
    for (const auto& block : style_blocks) {
        css.append("\n");
        css.append(block);
    }
    return css;
}

void BrowserApp::parse_and_apply_css(const std::string& css) {
    const auto css_parse_start = Hummingbird::Core::Clock::now();
    Hummingbird::Css::Parser css_parser(css);
    auto stylesheet = css_parser.parse();
    const auto css_parse_end = Hummingbird::Core::Clock::now();
    HB_LOG_INFO("[perf] css parse ms=" << Hummingbird::Core::duration_ms(css_parse_start, css_parse_end)
                                       << " rules=" << stylesheet.rules.size());

    const auto style_start = Hummingbird::Core::Clock::now();
    style_engine_.apply(stylesheet, dom_tree_.get());
    const auto style_end = Hummingbird::Core::Clock::now();
    HB_LOG_INFO("[pipeline] applied stylesheet rules: " << stylesheet.rules.size());
    HB_LOG_INFO("[perf] style apply ms=" << Hummingbird::Core::duration_ms(style_start, style_end));
}

bool BrowserApp::build_render_tree() {
    const auto render_start = Hummingbird::Core::Clock::now();
    render_tree_ = tree_builder_.build(dom_tree_.get());
    const auto render_end = Hummingbird::Core::Clock::now();
    if (!render_tree_ || !graphics_) {
        HB_LOG_WARN("[pipeline] render tree build skipped");
        return false;
    }
    HB_LOG_INFO("[perf] render tree build ms=" << Hummingbird::Core::duration_ms(render_start, render_end));
    return true;
}

void BrowserApp::layout_current_window() {
    auto [win_w, win_h] = window_->get_size();
    relayout_for_window(win_w, win_h);
}

void BrowserApp::render_if_needed() {
    if (!needs_repaint_ || !graphics_) return;

    auto [win_w, win_h] = window_->get_size();

    // Full viewport clear
    Hummingbird::Layout::Rect full{0, 0, static_cast<float>(win_w), static_cast<float>(win_h)};
    graphics_->set_viewport(full);
    graphics_->clear(kClearColor);

    // URL bar
    Hummingbird::Layout::Rect bar{0, 0, static_cast<float>(win_w), static_cast<float>(url_bar_height_)};
    graphics_->fill_rect(bar, kOverlayBg);

    TextStyle url_style;
    url_style.font_path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf").string();
    url_style.font_size = 16.0f;
    url_style.color = kOverlayText;

    graphics_->draw_text(url_bar_text_ + (url_bar_active_ ? "|" : ""), 8.0f, 8.0f, url_style);

    // Document paint
    if (render_tree_) {
        const int content_h = std::max(0, win_h - url_bar_height_);
        Hummingbird::Layout::Rect viewport{0.0f, static_cast<float>(url_bar_height_), static_cast<float>(win_w),
                                           static_cast<float>(content_h)};

        graphics_->set_viewport(viewport);

        Hummingbird::Renderer::PaintOptions opts;
        opts.debug_outlines = debug_outlines_;
        opts.scroll_y = scroll_y_;
        opts.viewport = viewport;

        const auto paint_start = Hummingbird::Core::Clock::now();
        painter_.paint(*render_tree_, *graphics_, opts);
        const auto paint_end = Hummingbird::Core::Clock::now();
        HB_LOG_DEBUG("[perf] paint ms=" << Hummingbird::Core::duration_ms(paint_start, paint_end)
                                        << " scroll_y=" << scroll_y_);
    }

    graphics_->present();
    needs_repaint_ = false;
}
