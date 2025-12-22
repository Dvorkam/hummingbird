#include <SDL.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/AssetPath.h"
#include "core/IGraphicsContext.h"
#include "core/Log.h"
#include "html/HtmlParser.h"
#include "layout/TreeBuilder.h"
#include "platform/CurlNetwork.h"
#include "platform/SDLWindow.h"
#include "platform/StubNetwork.h"
#include "renderer/Painter.h"
#include "style/Parser.h"
#include "style/StyleEngine.h"

int main(int argc, char* argv[]) {
    auto window = std::make_unique<SDLWindow>();
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

    std::string current_url = "www.acme.com";
    ArenaPtr<Hummingbird::DOM::Node> dom_tree;
    std::unique_ptr<Hummingbird::Layout::RenderObject> render_tree;

    std::mutex pending_mutex;
    std::optional<std::string> pending_html;
    ArenaAllocator dom_arena(2 * 1024 * 1024);

    bool url_bar_active = true;
    bool debug_outlines = false;
    float scroll_y = 0.0f;
    float content_height = 0.0f;

    auto load_url = [&](const std::string& url) {
        network.get(url, [&, url](std::string body) {
            if (body.empty()) {
                HB_LOG_WARN("[network] curl returned empty for " << url << ", using stub");
                stub.get(url, [&, url](std::string fallback) {
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

    int url_bar_height = 32;
    load_url(current_url);
    SDL_StartTextInput();
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
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                window->close();
                break;
            } else if (event.type == SDL_TEXTINPUT) {
                if (url_bar_active) {
                    current_url += event.text.text;
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_BACKSPACE && url_bar_active && !current_url.empty()) {
                    current_url.pop_back();
                } else if (event.key.keysym.sym == SDLK_RETURN) {
                    url_bar_active = false;
                    SDL_StopTextInput();
                    load_url(current_url);
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    url_bar_active = false;
                    SDL_StopTextInput();
                } else if (event.key.keysym.sym == SDLK_F1) {
                    debug_outlines = !debug_outlines;
                    HB_LOG_INFO("[ui] Debug outlines " << (debug_outlines ? "ON" : "OFF"));
                } else if (event.key.keysym.sym == SDLK_l && (event.key.keysym.mod & KMOD_CTRL)) {
                    url_bar_active = true;
                    SDL_StartTextInput();
                    HB_LOG_INFO("[ui] URL bar focused");
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                // Focus URL bar on click inside the bar area.
                int y = event.button.y;
                if (y < url_bar_height) {
                    url_bar_active = true;
                    SDL_StartTextInput();
                    HB_LOG_INFO("[ui] URL bar focused (mouse)");
                } else {
                    url_bar_active = false;
                    SDL_StopTextInput();
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                float delta = static_cast<float>(event.wheel.y) * 32.0f;
                scroll_y -= delta;
                if (scroll_y < 0.0f) scroll_y = 0.0f;
                auto [win_w, win_h] = window->get_size();
                float viewport_height = static_cast<float>(win_h - url_bar_height);
                float max_scroll = std::max(0.0f, content_height - viewport_height);
                if (scroll_y > max_scroll) scroll_y = max_scroll;
            }
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
                }
            }
        }

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
        graphics->draw_text(current_url + (url_bar_active ? "|" : ""), 8.0f, 8.0f, url_style);

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
        SDL_Delay(5);  // throttle to avoid busy-spin when idle
    }

    SDL_StopTextInput();
    window->close();

    return 0;
}
