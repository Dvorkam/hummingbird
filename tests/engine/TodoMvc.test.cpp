#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/InputEvent.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "engine/document/DocumentPipeline.h"
#include "engine/resources/ResourceStore.h"
#include "layout/RenderObject.h"
#include "layout/geometry/Geometry.h"

namespace {
using Hummingbird::Engine::DocumentPipeline;
using Hummingbird::Engine::ResourceStore;
using Hummingbird::Layout::Point;
using Hummingbird::Layout::Rect;
using Hummingbird::Layout::RenderObject;

// Records painted text so the flow can be asserted on what actually renders.
class RecordingGraphicsContext : public Hummingbird::IGraphicsContext {
public:
    void set_viewport(const Rect&) override {}
    void clear(const Hummingbird::Color&) override {}
    void present() override {}
    void fill_rect(const Rect&, const Hummingbird::Color&) override {}
    void draw_image(Hummingbird::ResourceRef, const Rect&) override {}
    void draw_image(const Hummingbird::ImageBitmap&, const Rect&) override {}
    Hummingbird::TextMetrics measure_text(const std::string& text, const Hummingbird::TextStyle& style) override {
        const float font_size = style.font_size > 0.0f ? style.font_size : 16.0f;
        Hummingbird::TextMetrics m;
        m.width = static_cast<float>(text.size()) * font_size * 0.5f;
        m.height = font_size;
        m.ascent = font_size * 0.8f;
        m.descent = font_size * 0.2f;
        return m;
    }
    void draw_text(const std::string& text, float, float, const Hummingbird::TextStyle&) override {
        drawn_texts.push_back(text);
    }
    std::vector<std::string> drawn_texts;
};

std::string read_fixture(const std::string& name) {
    std::ifstream file(std::string(HB_TEST_FIXTURE_DIR) + "/" + name, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool has_class(const RenderObject* node, std::string_view token) {
    const auto* el = dynamic_cast<const Hummingbird::DOM::Element*>(node->get_dom_node());
    if (!el) return false;
    const auto* attr = el->find_attribute("class");
    if (!attr) return false;
    std::string_view classes(*attr);
    size_t pos = 0;
    while (pos < classes.size()) {
        size_t end = classes.find(' ', pos);
        if (end == std::string_view::npos) end = classes.size();
        if (classes.substr(pos, end - pos) == token) return true;
        pos = end + 1;
    }
    return false;
}

// Absolute-space center of the first render box whose element carries `token`.
std::optional<Point> center_of(const RenderObject* node, std::string_view token, float ox = 0.0f, float oy = 0.0f) {
    if (!node) return std::nullopt;
    Rect r = node->get_rect();
    r.x += ox;
    r.y += oy;
    if (has_class(node, token)) {
        return Point{r.x + r.width * 0.5f, r.y + r.height * 0.5f};
    }
    for (const auto& child : node->get_children()) {
        if (auto p = center_of(child.get(), token, r.x, r.y)) return p;
    }
    return std::nullopt;
}
}  // namespace

// End-to-end regression harness for the pinned vanilla-JS TodoMVC fixture
// (tests/fixtures/todomvc, story 7.5.1): drive add -> toggle -> filter ->
// clear-completed -> delete headlessly through the real pipeline, so a
// regression in any of the underlying features (keydown add, checkbox change,
// hashchange routing, click delegation, DOM rebuild) fails CI.
TEST(TodoMvcTest, FullFlowDrivesThePinnedFixture) {
    const std::string html = read_fixture("todomvc/todomvc.html");
    const std::string js = read_fixture("todomvc/todomvc.js");
    ASSERT_FALSE(html.empty()) << "todomvc.html fixture missing";
    ASSERT_FALSE(js.empty()) << "todomvc.js fixture missing";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    const Rect viewport{0, 0, 800, 600};
    const std::string base = "https://todomvc.test/";

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.set_location(base);
    pipeline.apply_styles_and_layout(graphics, viewport, base);
    pipeline.run_scripts([&](std::string_view src) -> std::optional<std::string_view> {
        if (src == "todomvc.js") return std::string_view(js);
        return std::nullopt;
    });

    // Rebuilds layout, repaints, and reports whether `needle` appears in the
    // painted text. Inline text is emitted as per-word runs, so join them and
    // substring-search rather than matching a single draw call.
    const auto painted = [&](const char* needle) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, base);
        pipeline.paint(graphics, {viewport, false, 0.0f});
        std::string joined;
        for (const auto& t : graphics.drawn_texts) joined += t;
        return joined.find(needle) != std::string::npos;
    };

    // Types `title` into the (re-focused) new-todo input and presses Enter.
    const auto add_todo = [&](const std::string& title) {
        pipeline.apply_styles_and_layout(graphics, viewport, base);
        auto c = center_of(pipeline.render_root(), "new-todo");
        ASSERT_TRUE(c.has_value()) << "new-todo input not found";
        pipeline.focus_input_at({*c, viewport, base, 0.0f, 1});
        pipeline.handle_text_input(title);
        Hummingbird::InputEvent enter{};
        enter.type = Hummingbird::EventType::KeyDown;
        enter.key.key = Hummingbird::Key::Enter;
        pipeline.handle_key_down(enter, base);
    };

    // Clicks the center of the first element carrying `cls`; returns whether the
    // click mutated the DOM. Layout is refreshed first so the hit-test is current.
    const auto click_first = [&](const char* cls) -> bool {
        pipeline.apply_styles_and_layout(graphics, viewport, base);
        auto c = center_of(pipeline.render_root(), cls);
        if (!c) return false;
        return pipeline.dispatch_click({*c, viewport, base, 0.0f, 1}).mutated;
    };

    // --- empty state ---
    EXPECT_FALSE(painted("1 item left"));  // footer hidden while there are no todos

    // --- add two todos via the keyboard ---
    add_todo("Write a browser");
    add_todo("Ship milestone 7");
    EXPECT_TRUE(painted("Write a browser"));
    EXPECT_TRUE(painted("Ship milestone 7"));
    EXPECT_TRUE(painted("2 items left"));

    // --- toggle the first todo complete via its checkbox ---
    EXPECT_TRUE(click_first("toggle")) << "clicking the toggle checkbox did nothing";
    EXPECT_TRUE(painted("1 item left"));  // one of two now completed

    // --- hash-routed filters ---
    pipeline.navigate_fragment(base + "#/active");
    EXPECT_FALSE(painted("Write a browser"));  // completed -> hidden under Active
    EXPECT_TRUE(painted("Ship milestone 7"));  // active -> shown

    pipeline.navigate_fragment(base + "#/completed");
    EXPECT_TRUE(painted("Write a browser"));    // completed -> shown
    EXPECT_FALSE(painted("Ship milestone 7"));  // active -> hidden

    pipeline.navigate_fragment(base + "#/");
    EXPECT_TRUE(painted("Write a browser"));
    EXPECT_TRUE(painted("Ship milestone 7"));

    // --- clear completed removes the toggled todo ---
    EXPECT_TRUE(click_first("clear-completed"));
    EXPECT_FALSE(painted("Write a browser"));  // was completed -> cleared
    EXPECT_TRUE(painted("Ship milestone 7"));
    EXPECT_TRUE(painted("1 item left"));

    // --- delete the last todo via its destroy button -> empty state ---
    EXPECT_TRUE(click_first("destroy"));
    EXPECT_FALSE(painted("Ship milestone 7"));
    EXPECT_FALSE(painted("1 item left"));  // footer hidden again
}
