#include "renderer/Painter.h"

#include <gtest/gtest.h>

#include <cmath>
#include <functional>

#include "core/dom/Element.h"
#include "core/platform_api/IGraphicsContext.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderListItem.h"
#include "layout/TreeBuilder.h"
#include "style/CssParser.h"
#include "style/StyleEngine.h"

using Hummingbird::Layout::RenderObject;
using namespace Hummingbird::Css;

// Graphics context that records draw_text calls and provides deterministic metrics.
class RecordingGraphicsContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& viewport) override { viewport_ = viewport; }
    void clear(const Color&) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& rect, const Color&) override { fill_calls.push_back(rect); }

    TextMetrics measure_text(const std::string& text, const TextStyle&) override {
        return {static_cast<float>(text.size()) * 8.0f, 16.0f};
    }

    void draw_text(const std::string& text, float, float, const TextStyle&) override {
        ++draw_calls;
        last_text = text;
    }

    int draw_calls = 0;
    std::string last_text;
    std::vector<Hummingbird::Layout::Rect> fill_calls;
    Hummingbird::Layout::Rect viewport_{0, 0, 0, 0};
};

TEST(PainterIntegrationTest, PaintsTextNodesFromParserOutput) {
    // Arrange: parse a simple HTML snippet.
    std::string_view html = "<html><body><p>First line</p><p>Second line</p></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    // Build render tree.
    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    // Layout and paint with recording context.
    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 800, 600};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    // Expect one draw call per tokenized run.
    EXPECT_EQ(context.draw_calls, 6);
    EXPECT_EQ(context.last_text, "line");
}

TEST(PainterDebugTest, DrawsOutlinesWhenDebugEnabled) {
    // DOM: <html><body><p>Text</p></body></html>
    std::string_view html = "<html><body><p>Text</p></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.debug_outlines = true;
    painter.paint(*render_tree, context, opts);

    // Expect at least one fill_rect call for debug outlines.
    EXPECT_FALSE(context.fill_calls.empty());
}

TEST(PainterTest, PaintsBordersFromComputedStyle) {
    std::string_view html = "<html><body><div>Box</div></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "div { border-width: 2px; border-style: solid; border-color: red; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    EXPECT_GE(context.fill_calls.size(), 4u);
}

namespace {
bool rect_matches(const Hummingbird::Layout::Rect& a, const Hummingbird::Layout::Rect& b) {
    constexpr float kEpsilon = 0.01f;
    return std::fabs(a.x - b.x) < kEpsilon && std::fabs(a.y - b.y) < kEpsilon &&
           std::fabs(a.width - b.width) < kEpsilon && std::fabs(a.height - b.height) < kEpsilon;
}

Hummingbird::Layout::Rect absolute_rect_for(const Hummingbird::Layout::RenderObject* node) {
    Hummingbird::Layout::Rect rect = node->get_rect();
    const auto* parent = node->get_parent();
    while (parent) {
        const auto& parent_rect = parent->get_rect();
        rect.x += parent_rect.x;
        rect.y += parent_rect.y;
        parent = parent->get_parent();
    }
    return rect;
}

Hummingbird::Layout::RenderObject* find_tag(Hummingbird::Layout::RenderObject* node, std::string_view tag) {
    if (!node) return nullptr;
    if (auto* element = dynamic_cast<const Hummingbird::DOM::Element*>(node->get_dom_node())) {
        if (element->get_tag_name() == tag) {
            return node;
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* found = find_tag(child.get(), tag)) {
            return found;
        }
    }
    return nullptr;
}
}  // namespace

TEST(PainterTest, PaintsBorderEdgesAtComputedPositions) {
    std::string_view html = "<html><body><div>Box</div></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "div { border-width: 2px; border-style: solid; border-color: red; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    auto* div_node = find_tag(render_tree.get(), Hummingbird::Html::TagNames::Div);
    ASSERT_NE(div_node, nullptr);
    const auto* style = div_node->get_computed_style();
    ASSERT_NE(style, nullptr);

    const auto absolute = absolute_rect_for(div_node);
    const auto& bw = style->border_width;

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    ASSERT_GE(context.fill_calls.size(), 4u);
    Hummingbird::Layout::Rect expected_top{absolute.x, absolute.y, absolute.width, bw.top};
    Hummingbird::Layout::Rect expected_bottom{absolute.x, absolute.y + absolute.height - bw.bottom, absolute.width,
                                              bw.bottom};
    Hummingbird::Layout::Rect expected_left{absolute.x, absolute.y, bw.left, absolute.height};
    Hummingbird::Layout::Rect expected_right{absolute.x + absolute.width - bw.right, absolute.y, bw.right,
                                             absolute.height};

    bool found_top = false;
    bool found_bottom = false;
    bool found_left = false;
    bool found_right = false;

    for (const auto& rect : context.fill_calls) {
        found_top = found_top || rect_matches(rect, expected_top);
        found_bottom = found_bottom || rect_matches(rect, expected_bottom);
        found_left = found_left || rect_matches(rect, expected_left);
        found_right = found_right || rect_matches(rect, expected_right);
    }

    EXPECT_TRUE(found_top);
    EXPECT_TRUE(found_bottom);
    EXPECT_TRUE(found_left);
    EXPECT_TRUE(found_right);
}

TEST(PainterTest, PaintsBackgroundForBoxes) {
    std::string_view html = "<html><body><div>Box</div></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "div { background-color: #cccccc; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    auto* div_node = find_tag(render_tree.get(), Hummingbird::Html::TagNames::Div);
    ASSERT_NE(div_node, nullptr);
    const auto expected = absolute_rect_for(div_node);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    bool found = false;
    for (const auto& rect : context.fill_calls) {
        if (rect_matches(rect, expected)) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(PainterTest, PaintsImagePlaceholderWithAltText) {
    std::string_view html = "<html><body><img alt=\"Logo\" width=\"32\" height=\"16\"></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    EXPECT_EQ(context.draw_calls, 1);
    EXPECT_EQ(context.last_text, "Logo");
    EXPECT_GE(context.fill_calls.size(), 4u);
}

TEST(PainterTest, SkipsPaintForOffscreenNodes) {
    std::string_view html = "<html><body><p>First</p><p>Second</p></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 15};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    EXPECT_EQ(context.draw_calls, 1);
    EXPECT_EQ(context.last_text, "First");
}

TEST(PainterTest, PaintsListMarkersWithCulling) {
    std::string_view html = "<html><body><ul><li>Item</li></ul></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Css::Stylesheet sheet;
    Hummingbird::Css::StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Layout::RenderListItem* list_item = nullptr;
    std::function<void(Hummingbird::Layout::RenderObject*)> find_list_item =
        [&](Hummingbird::Layout::RenderObject* node) {
            if (!node || list_item) return;
            if (auto* item = dynamic_cast<Hummingbird::Layout::RenderListItem*>(node)) {
                list_item = item;
                return;
            }
            for (const auto& child : node->get_children()) {
                find_list_item(child.get());
            }
        };
    find_list_item(render_tree.get());
    ASSERT_NE(list_item, nullptr);
    EXPECT_GT(list_item->marker_rect().width, 0.0f);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    EXPECT_FALSE(context.fill_calls.empty());
}

TEST(PainterTest, PaintsHorizontalRuleWithCulling) {
    std::string_view html = "<html><body><hr></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    EXPECT_FALSE(context.fill_calls.empty());
}
