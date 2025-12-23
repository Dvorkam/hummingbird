
#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/AssetPath.h"
#include "core/IGraphicsContext.h"
#include "core/IWindow.h"
#include "core/InputEvent.h"
#include "core/Log.h"
#include "core/WindowFactory.h"
#include "html/HtmlParser.h"
#include "layout/TreeBuilder.h"
#include "platform/CurlNetwork.h"
#include "platform/StubNetwork.h"
#include "renderer/Painter.h"
#include "style/Parser.h"
#include "style/StyleEngine.h"

int main(int argc, char* argv[]) {
    auto window = create_window();
    window->open();

    if (!window->is_open()) {
        return 1;  // Exit if window creation failed
    }

    auto graphics = window->get_graphics_context();
    if (!graphics) {
        return 1;
    }
    CurlNetwork network;
    StubNetwork stub;
    Hummingbird::Css::StyleEngine style_engine;
    Hummingbird::Layout::TreeBuilder tree_builder;
    Hummingbird::Renderer::Painter painter;

    std::string url_bar_text = "www.acme.com";
    std::string requested_url = url_bar_text;
    ArenaPtr<Hummingbird::DOM::Node> dom_tree;
    std::unique_ptr<Hummingbird::Layout::RenderObject> render_tree;

    std::mutex pending_mutex;
    std::optional<std::string> pending_html;
    ArenaAllocator dom_arena(2 * 1024 * 1024);

    bool url_bar_active = true;
    bool debug_outlines = false;
    float scroll_y = 0.0f;
    float content_height = 0.0f;
    std::atomic<uint64_t> nav_counter{0};
    std::atomic<uint64_t> active_nav{0};
    int url_bar_height = 32;

    bool needs_repaint = true;

    auto clamp_scroll = [&](float viewport_height) {
        float max_scroll = std::max(0.0f, content_height - viewport_height);
        if (scroll_y < 0.0f) scroll_y = 0.0f;
        if (scroll_y > max_scroll) scroll_y = max_scroll;
    };

    auto load_url = [&](const std::string& url) {
        uint64_t id = ++nav_counter;
        active_nav.store(id, std::memory_order_relaxed);

        requested_url = url;
        network.get(url, [&, id, url](std::string body) {
            if (id != active_nav.load(std::memory_order_relaxed)) return;
            if (body.empty()) {
                HB_LOG_WARN("[network] curl returned empty for " << url << ", using stub");
                stub.get(url, [&, id, url](std::string fallback) {
                    if (id != active_nav.load(std::memory_order_relaxed)) return;
                    std::lock_guard<std::mutex> lg(pending_mutex);
                    pending_html = std::move(fallback);
                });
                return;
            }
            HB_LOG_INFO("[network] fetched " << body.size() << " bytes from " << url);
            std::lock_guard<std::mutex> lg(pending_mutex);
            pending_html = std::move(body);
        });
    };

    auto handle_event = [&](const InputEvent& event) {
        switch (event.type) {
            case EventType::Quit: {
                window->close();
                break;
            }

            case EventType::TextInput: {
                if (url_bar_active) {
                    url_bar_text += event.text.text;
                    needs_repaint = true;
                }
                break;
            }

            case EventType::KeyDown: {
                if (event.key.key == Key::Backspace && url_bar_active && !url_bar_text.empty()) {
                    url_bar_text.pop_back();
                } else if (event.key.key == Key::Enter) {
                    url_bar_active = false;
                    window->stop_text_input();
                    load_url(url_bar_text);
                } else if (event.key.key == Key::Escape) {
                    url_bar_active = false;
                    window->stop_text_input();
                } else if (event.key.key == Key::F1) {
                    debug_outlines = !debug_outlines;
                    HB_LOG_INFO("[ui] Debug outlines " << (debug_outlines ? "ON" : "OFF"));
                } else if (event.key.key == Key::L && (event.mods.ctrl)) {
                    url_bar_active = true;
                    window->start_text_input();
                    HB_LOG_INFO("[ui] URL bar focused");
                }
                needs_repaint = true;
                break;
            }
            case EventType::MouseDown: {
                int y = event.mouse_button.y;
                if (y < url_bar_height) {
                    url_bar_active = true;
                    window->start_text_input();
                    HB_LOG_INFO("[ui] URL bar focused (mouse)");
                } else {
                    url_bar_active = false;
                    window->stop_text_input();
                }
                needs_repaint = true;
                break;
            }
            case EventType::MouseWheel: {
                float delta = static_cast<float>(event.wheel.dy) * 32.0f;
                scroll_y -= delta;
                auto [win_w, win_h] = window->get_size();
                float viewport_height = static_cast<float>(win_h - url_bar_height);
                clamp_scroll(viewport_height);
                needs_repaint = true;
                break;
            }

            case EventType::Resize: {
                if (render_tree) {
                    const int win_w = event.resize.width;
                    const int win_h = event.resize.height;
                    Hummingbird::Layout::Rect viewport = {0, static_cast<float>(url_bar_height),
                                                          static_cast<float>(win_w),
                                                          static_cast<float>(win_h - url_bar_height)};
                    render_tree->layout(*graphics, viewport);
                    content_height = render_tree->get_rect().height;
                    clamp_scroll(viewport.height);
                }
                needs_repaint = true;

                break;
            }

            default:
                break;
        }
    };

    load_url(url_bar_text);
    window->start_text_input();
    HB_LOG_INFO("[ui] URL bar focused (default)");

    const Color teal = {255, 255, 255, 255};
    Color overlay_bg{220, 220, 220, 255};
    Color overlay_text{0, 0, 0, 255};

    HB_LOG_INFO("[ui] Starting app. Press Ctrl+L to focus URL bar.");

    std::function<size_t(const Hummingbird::DOM::Node*)> count_nodes =
        [&](const Hummingbird::DOM::Node* node) -> size_t {
        if (!node) return 0;
        size_t total = 1;
        for (const auto& child : node->get_children()) {
            total += count_nodes(child.get());
        }
        return total;
    };

    while (window->is_open()) {
        window->update();
        InputEvent e;
        if (window->wait_event(e, 16)) {
            handle_event(e);
        }

        // Drain the rest without blocking
        int processed = 0;
        while (processed++ < 200 && window->poll_event(e)) {
            handle_event(e);
        }

        if (!window->is_open()) {
            break;
        }

        // Consume pending HTML and rebuild on the main thread.
        {
            std::optional<std::string> html_copy;
            {
                std::lock_guard<std::mutex> lg(pending_mutex);
                if (pending_html.has_value()) {
                    html_copy = std::move(pending_html);
                    pending_html.reset();
                }
            }
            if (html_copy.has_value()) {
                // Drop old trees before resetting arena to avoid dangling destroy_at.
                dom_tree.reset();
                render_tree.reset();
                dom_arena.reset();
                HB_LOG_INFO("[pipeline] html size: " << html_copy->size());
                Hummingbird::Html::Parser parser(dom_arena, *html_copy);
                dom_tree = parser.parse();
                HB_LOG_INFO("[pipeline] parsed DOM children: " << dom_tree->get_children().size()
                                                               << " total nodes: " << count_nodes(dom_tree.get()));
                std::string css = "body { padding: 8px; } p { margin: 4px; }";
                Hummingbird::Css::Parser css_parser(css);
                auto stylesheet = css_parser.parse();
                style_engine.apply(stylesheet, dom_tree.get());
                HB_LOG_INFO("[pipeline] applied stylesheet rules: " << stylesheet.rules.size());
                render_tree = tree_builder.build(dom_tree.get());
                if (render_tree) {
                    auto [win_w, win_h] = window->get_size();
                    Hummingbird::Layout::Rect viewport = {0, static_cast<float>(url_bar_height),
                                                          static_cast<float>(win_w),
                                                          static_cast<float>(win_h - url_bar_height)};
                    render_tree->layout(*graphics, viewport);
                    content_height = render_tree->get_rect().height;
                    HB_LOG_INFO("[pipeline] render tree root children: " << render_tree->get_children().size());
                    needs_repaint = true;
                    clamp_scroll(viewport.height);
                }
            }
        }

        if (needs_repaint) {
            auto [win_w, win_h] = window->get_size();
            Hummingbird::Layout::Rect full_viewport{0, 0, static_cast<float>(win_w), static_cast<float>(win_h)};
            graphics->set_viewport(full_viewport);
            graphics->clear(teal);

            // Draw URL bar overlay.
            Hummingbird::Layout::Rect bar_rect{0, 0, static_cast<float>(win_w), static_cast<float>(url_bar_height)};
            graphics->fill_rect(bar_rect, overlay_bg);
            auto font_path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf").string();
            TextStyle url_style;
            url_style.font_path = font_path;
            url_style.font_size = 16.0f;
            url_style.color = overlay_text;
            graphics->draw_text(url_bar_text + (url_bar_active ? "|" : ""), 8.0f, 8.0f, url_style);

            // Paint the Render Tree
            if (render_tree) {
                Hummingbird::Layout::Rect viewport = {0, static_cast<float>(url_bar_height), static_cast<float>(win_w),
                                                      static_cast<float>(win_h - url_bar_height)};
                graphics->set_viewport(viewport);
                Hummingbird::Renderer::PaintOptions opts;
                opts.debug_outlines = debug_outlines;
                opts.scroll_y = scroll_y;
                opts.viewport = viewport;
                painter.paint(*render_tree, *graphics, opts);
            }

            graphics->present();
            needs_repaint = false;
        }
    }

    window->stop_text_input();
    window->close();

    return 0;
}
