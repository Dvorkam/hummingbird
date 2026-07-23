#include "engine/document/DocumentPipeline.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/InputEvent.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "engine/document/DocumentModel.h"
#include "engine/document/DocumentScripting.h"
#include "engine/resources/ResourceStore.h"
#include "layout/RenderObject.h"
#include "layout/flow/TextStyleUtils.h"
#include "layout/geometry/Geometry.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
using Hummingbird::ImageBitmap;
using Hummingbird::PixelFormat;
using Hummingbird::Engine::DocumentPipeline;
using Hummingbird::Engine::ResourceStore;
using Hummingbird::Engine::ResourceType;
using Hummingbird::Layout::Point;
using Hummingbird::Layout::Rect;
using Hummingbird::Test::TestGraphicsContext;

class RecordingGraphicsContext : public Hummingbird::IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& /*viewport*/) override {}
    void clear(const Hummingbird::Color& /*color*/) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& /*rect*/, const Hummingbird::Color& /*color*/) override {}
    void draw_image(const ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override { ++image_calls; }

    Hummingbird::TextMetrics measure_text(const std::string& text, const Hummingbird::TextStyle& style) override {
        const float font_size = style.font_size > 0.0f ? style.font_size : 16.0f;
        const float average_char_width = font_size * 0.5f;
        Hummingbird::TextMetrics metrics;
        metrics.width = static_cast<float>(text.size()) * average_char_width;
        metrics.height = font_size;
        metrics.ascent = font_size * 0.8f;
        metrics.descent = font_size * 0.2f;
        metrics.underline_position = -metrics.descent * 0.5f;
        metrics.underline_thickness = 1.0f;
        return metrics;
    }

    void draw_text(const std::string& text, float /*x*/, float /*y*/,
                   const Hummingbird::TextStyle& /*style*/) override {
        drawn_texts.push_back(text);
    }

    int image_calls = 0;
    std::vector<std::string> drawn_texts;
};

// Absolute-space center of the first render box whose element carries `token`.
std::optional<Point> center_by_class(const Hummingbird::Layout::RenderObject* node, std::string_view token,
                                     float ox = 0.0f, float oy = 0.0f) {
    if (!node) return std::nullopt;
    Rect r = node->get_rect();
    r.x += ox;
    r.y += oy;
    const auto* el = dynamic_cast<const Hummingbird::DOM::Element*>(node->get_dom_node());
    if (el) {
        const auto* attr = el->find_attribute("class");
        if (attr) {
            std::string_view classes(*attr);
            size_t pos = 0;
            while (pos < classes.size()) {
                size_t end = classes.find(' ', pos);
                if (end == std::string_view::npos) end = classes.size();
                if (classes.substr(pos, end - pos) == token) {
                    return Point{r.x + r.width * 0.5f, r.y + r.height * 0.5f};
                }
                pos = end + 1;
            }
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto p = center_by_class(child.get(), token, r.x, r.y)) return p;
    }
    return std::nullopt;
}
}  // namespace

TEST(DocumentPipelineTest, DispatchesLoadHandler) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      body { margin: 0; padding: 0; }
    </style>
  </head>
  <body onload="const target = document.getElementById('status'); if (target) { target.textContent = 'loaded'; }">
    <p id="status">idle</p>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    auto result = pipeline.dispatch_load();
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.mutated);
}

TEST(DocumentPipelineTest, ClassToggleRestylesElementHidden) {
    // A JS class toggle must feed selector re-match: adding `.hidden`
    // (display:none) removes the element from the rendered output (7.1.2).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      body { margin: 0; padding: 0; }
      .hidden { display: none; }
    </style>
  </head>
  <body onload="document.getElementById('x').classList.add('hidden');">
    <p id="x">visibletext</p>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.paint(graphics, {viewport, false, 0.0f});
    const auto contains_text = [&] {
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), "visibletext") !=
               graphics.drawn_texts.end();
    };
    EXPECT_TRUE(contains_text());  // visible before the toggle

    auto load = pipeline.dispatch_load();
    ASSERT_TRUE(load.handled);
    ASSERT_TRUE(load.mutated);

    graphics.drawn_texts.clear();
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.paint(graphics, {viewport, false, 0.0f});
    EXPECT_FALSE(contains_text());  // .hidden { display:none } restyled it away
}

TEST(DocumentModelTest, MarksAnchorsVisitedByResolvedHref) {
    using Hummingbird::DOM::Element;
    using Hummingbird::DOM::Node;
    using Hummingbird::Engine::DocumentModel;

    DocumentModel model;
    ASSERT_TRUE(model.parse_html("<html><body><a href=\"/next\">n</a><a href=\"/other\">o</a></body></html>").ok);

    std::unordered_set<std::string> visited{"https://example.dev/next"};
    model.mark_visited_links(visited, "https://example.dev/page");

    std::vector<Element*> anchors;
    std::function<void(Node*)> walk = [&](Node* node) {
        if (auto* element = dynamic_cast<Element*>(node)) {
            if (element->get_tag_name() == "a") anchors.push_back(element);
        }
        for (const auto& child : node->get_children()) walk(child.get());
    };
    walk(model.dom_root());

    ASSERT_EQ(anchors.size(), 2u);
    // /next resolves to the visited URL; /other does not.
    EXPECT_TRUE(anchors[0]->has_pseudo_state(Element::PseudoState::Visited));
    EXPECT_FALSE(anchors[1]->has_pseudo_state(Element::PseudoState::Visited));
}

TEST(DocumentModelTest, FlushesCountedCompatibilitySummary) {
    using Hummingbird::DOM::Element;
    using Hummingbird::DOM::Node;
    using Hummingbird::Engine::DocumentModel;

    DocumentModel model;
    ASSERT_TRUE(model.parse_html("<html><body><p>hello</p></body></html>").ok);
    model.apply_styles("p { font-family: montserrat; }");

    Element* paragraph = nullptr;
    std::function<void(Node*)> find_paragraph = [&](Node* node) {
        if (auto* element = dynamic_cast<Element*>(node); element && element->get_tag_name() == "p") {
            paragraph = element;
            return;
        }
        for (const auto& child : node->get_children()) {
            if (!paragraph) find_paragraph(child.get());
        }
    };
    find_paragraph(model.dom_root());
    ASSERT_NE(paragraph, nullptr);
    ASSERT_TRUE(paragraph->get_computed_style());

    std::stringstream buffer;
    auto* old = std::cerr.rdbuf(buffer.rdbuf());
    (void)Hummingbird::Layout::TextStyleUtils::resolve_text_font_path(paragraph->get_computed_style().get());
    (void)Hummingbird::Layout::TextStyleUtils::resolve_text_font_path(paragraph->get_computed_style().get());
    (void)Hummingbird::Layout::TextStyleUtils::resolve_text_font_path(paragraph->get_computed_style().get());
    model.flush_compatibility_warnings("https://example.dev/fonts");
    model.flush_compatibility_warnings("https://example.dev/fonts");
    std::cerr.rdbuf(old);

#if HB_LOG_LEVEL >= 2
    const std::string output = buffer.str();
    EXPECT_NE(output.find("[style] Unsupported font family list 'montserrat'"), std::string::npos);
    EXPECT_NE(output.find("[compat-summary] https://example.dev/fonts: 3 occurrences, 1 unique, 2 suppressed"),
              std::string::npos);
    EXPECT_NE(output.find("[compat-summary] 3x [style] Unsupported font family list 'montserrat'"), std::string::npos);
#else
    EXPECT_TRUE(buffer.str().empty());
#endif
}

TEST(DocumentModelTest, BudgetExhaustionShowsErrorPage) {
    using Hummingbird::DOM::Element;
    using Hummingbird::DOM::Node;
    using Hummingbird::Engine::DocumentModel;

    // A tiny single-block arena: big enough for the built-in error page, far too
    // small for a large document.
    DocumentModel model(8192, 1);
    std::string big = "<html><body>";
    for (int i = 0; i < 4000; ++i) {
        big += "<div>x</div>";
    }
    big += "</body></html>";

    auto result = model.parse_html(big);
    // The over-budget parse recovers into the error page rather than a blank tab.
    EXPECT_TRUE(result.ok);
    EXPECT_FALSE(result.arena_failed);
    ASSERT_NE(model.dom_root(), nullptr);

    // The rendered DOM is the built-in "too large" page: locate its <h1>.
    std::function<Element*(Node*)> find_h1 = [&](Node* node) -> Element* {
        if (auto* element = dynamic_cast<Element*>(node)) {
            if (element->get_tag_name() == "h1") return element;
        }
        for (const auto& child : node->get_children()) {
            if (auto* found = find_h1(child.get())) return found;
        }
        return nullptr;
    };
    EXPECT_NE(find_h1(model.dom_root()), nullptr);
}

TEST(DocumentPipelineTest, DetectsMediaBreakpointCrossOnResize) {
    // DDG gates its desktop layout behind (min-width: 864px)-style rules;
    // resizing across such a bound must trigger a restyle (T-MEDIA-RESIZE-1).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      body { margin: 0; }
      @media (min-width: 864px) {
        body { margin: 8px; }
      }
    </style>
  </head>
  <body><p>hi</p></body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect narrow{0, 0, 800, 600};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, narrow, "https://example.dev");

    // Same side of the breakpoint: relayout is enough.
    EXPECT_FALSE(pipeline.needs_restyle_for_viewport({0, 0, 820, 600}));
    // Crossing 864px flips the rule: restyle required.
    EXPECT_TRUE(pipeline.needs_restyle_for_viewport({0, 0, 1024, 768}));

    // After restyling at the wide viewport, staying wide needs no restyle
    // but shrinking back across the bound does.
    pipeline.apply_styles_and_layout(graphics, {0, 0, 1024, 768}, "https://example.dev");
    EXPECT_FALSE(pipeline.needs_restyle_for_viewport({0, 0, 900, 700}));
    EXPECT_TRUE(pipeline.needs_restyle_for_viewport(narrow));
}

TEST(DocumentPipelineTest, DispatchesClickHandler) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      body { margin: 0; padding: 0; }
      button { display: block; }
    </style>
  </head>
  <body>
    <button onclick="const target = document.getElementById('status'); if (target) { target.textContent = 'clicked'; }">
      Click
    </button>
    <p id="status">idle</p>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    DocumentPipeline::HitTestContext context{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f};
    auto result = pipeline.dispatch_click(context);
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.mutated);
}

TEST(DocumentPipelineTest, ClickDelegationAndDblclickThroughDispatch) {
    // A click listener on the container fires for a click on its child (bubbling),
    // and a double-click routes a dblclick event (7.2.4.1).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style>
    body { margin: 0; padding: 0; }
    div, button, p { display: block; }
  </style></head>
  <body>
    <div id="list"><button id="btn">X</button></div>
    <p id="status">idle</p>
    <script>
      document.getElementById('list').addEventListener('click', function() {
        document.getElementById('status').textContent = 'clickeddelegated';
      });
      document.getElementById('list').addEventListener('dblclick', function() {
        document.getElementById('status').textContent = 'doubleclicked';
      });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();  // registers the delegated listeners (no DOM mutation, so ignore the flag)

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    // Single click over the button (top-left) bubbles up to #list.
    DocumentPipeline::HitTestContext single{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 1};
    auto r1 = pipeline.dispatch_click(single);
    EXPECT_TRUE(r1.mutated);
    EXPECT_TRUE(painted_has("clickeddelegated"));

    // Double click additionally fires dblclick.
    DocumentPipeline::HitTestContext dbl{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 2};
    auto r2 = pipeline.dispatch_click(dbl);
    EXPECT_TRUE(r2.mutated);
    EXPECT_TRUE(painted_has("doubleclicked"));
}

TEST(DocumentPipelineTest, ClickPreventDefaultIsReported) {
    // A click listener that calls preventDefault is reported so the caller can
    // suppress the default action (link navigation).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style>
    body { margin: 0; padding: 0; }
    a { display: block; }
  </style></head>
  <body>
    <a id="lnk" href="/next">Link</a>
    <script>
      document.getElementById('lnk').addEventListener('click', function(e) { e.preventDefault(); });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();

    DocumentPipeline::HitTestContext ctx{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 1};
    auto result = pipeline.dispatch_click(ctx);
    EXPECT_TRUE(result.default_prevented);
}

TEST(DocumentPipelineTest, KeydownAndKeyupRouteToDocumentWithKeyFields) {
    // A document keydown/keyup listener sees the key value and can mutate the DOM
    // (7.2.4.2).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } p { display: block; } </style></head>
  <body>
    <p id="status">idle</p>
    <script>
      document.addEventListener('keydown', function(e) {
        document.getElementById('status').textContent = 'down:' + e.key;
      });
      document.addEventListener('keyup', function(e) {
        document.getElementById('status').textContent = 'up:' + e.key;
      });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    Hummingbird::InputEvent enter{};
    enter.type = Hummingbird::EventType::KeyDown;
    enter.key.key = Hummingbird::Key::Enter;
    auto down = pipeline.handle_key_down(enter, "https://example.dev");
    EXPECT_TRUE(down.mutated);
    EXPECT_TRUE(painted_has("down:Enter"));

    Hummingbird::InputEvent up{};
    up.type = Hummingbird::EventType::KeyUp;
    up.key.key = Hummingbird::Key::A;
    auto keyup = pipeline.handle_key_up(up);
    EXPECT_TRUE(keyup.mutated);
    EXPECT_TRUE(painted_has("up:a"));
}

TEST(DocumentPipelineTest, KeydownCarriesKeyCodeForDigitsSpaceAndPunctuation) {
    // Digits, space, and punctuation carry real key/code values, not key: ""
    // (T-KEY-FIELDS-COVERAGE-1, 7.7.4). `key` is the unshifted character; `code`
    // uses the UI Events code names.
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } p { display: block; } </style></head>
  <body>
    <p id="status">idle</p>
    <script>
      document.addEventListener('keydown', function(e) {
        // The painter uses whitespace as the run separator, so a raw space in
        // e.key is unobservable via painted text; map it to a sentinel (which
        // also confirms e.key === ' ') and use '_' as the separator.
        var k = e.key === ' ' ? 'SPACE' : e.key;
        document.getElementById('status').textContent = k + '_' + e.code;
      });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    const auto press = [&](Hummingbird::Key key) {
        Hummingbird::InputEvent event{};
        event.type = Hummingbird::EventType::KeyDown;
        event.key.key = key;
        pipeline.handle_key_down(event, "https://example.dev");
    };

    press(Hummingbird::Key::Num1);
    EXPECT_TRUE(painted_has("1_Digit1"));
    press(Hummingbird::Key::Num0);
    EXPECT_TRUE(painted_has("0_Digit0"));
    press(Hummingbird::Key::Space);  // e.key === ' ' → sentinel, code "Space"
    EXPECT_TRUE(painted_has("SPACE_Space"));
    press(Hummingbird::Key::Minus);
    EXPECT_TRUE(painted_has("-_Minus"));
    press(Hummingbird::Key::Period);
    EXPECT_TRUE(painted_has("._Period"));
    press(Hummingbird::Key::Slash);
    EXPECT_TRUE(painted_has("/_Slash"));
}

TEST(DocumentPipelineTest, KeydownPreventDefaultSuppressesTextEdit) {
    // preventDefault in a keydown listener stops the default text-edit action
    // (here: Backspace does not delete a character).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } input { display: block; } </style></head>
  <body>
    <input id="in" value="ab">
    <script>
      document.getElementById('in').addEventListener('keydown', function(e) {
        if (e.key === 'Backspace') { e.preventDefault(); }
      });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();

    // Focus the input (top-left), then press Backspace.
    DocumentPipeline::HitTestContext focus{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 1};
    ASSERT_TRUE(pipeline.focus_input_at(focus));

    Hummingbird::InputEvent backspace{};
    backspace.type = Hummingbird::EventType::KeyDown;
    backspace.key.key = Hummingbird::Key::Backspace;
    pipeline.handle_key_down(backspace, "https://example.dev");

    // preventDefault stopped the delete: the value is untouched.
    auto value = pipeline.focused_input_value();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "ab");
}

TEST(DocumentPipelineTest, FormInputLifecycleEvents) {
    // input on edit, change + blur on commit, focus on gain (7.2.4.3).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } input, p { display: block; } </style></head>
  <body>
    <input id="in" value="">
    <p id="log">start</p>
    <script>
      var el = document.getElementById('in');
      var log = document.getElementById('log');
      el.addEventListener('focus', function() { log.textContent = 'focus'; });
      el.addEventListener('input', function(e) { log.textContent = 'input:' + e.target.value; });
      el.addEventListener('change', function() { log.textContent = 'change'; });
      el.addEventListener('blur', function() { log.textContent = log.textContent + '+blur'; });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    // Focus fires on gain.
    DocumentPipeline::HitTestContext focus{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 1};
    ASSERT_TRUE(pipeline.focus_input_at(focus));
    EXPECT_TRUE(painted_has("focus"));

    // Typing fires input with the new value.
    pipeline.handle_text_input("hi");
    EXPECT_TRUE(painted_has("input:hi"));

    // Blurring a changed field fires change then blur.
    ASSERT_TRUE(pipeline.clear_input_focus());
    EXPECT_TRUE(painted_has("change+blur"));
}

TEST(DocumentPipelineTest, SubmitEventPreventDefaultReported) {
    // A submit listener that calls preventDefault is reported so the caller can
    // suppress the navigation (7.2.4.4).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } form, input, button { display: block; } </style></head>
  <body>
    <form id="f" action="/submit">
      <button id="go" type="submit">Go</button>
      <input id="q" value="hello">
    </form>
    <script>
      document.getElementById('f').addEventListener('submit', function(e) {
        e.preventDefault();
        document.getElementById('q').setAttribute('data-submitted', '1');
      });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();

    // A click on the submit button produces a FormSubmission carrying the form.
    DocumentPipeline::HitTestContext click{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 1};
    auto submission = pipeline.submit_form_at(click);
    ASSERT_TRUE(submission.has_value());
    ASSERT_NE(submission->form_element, nullptr);

    // Firing the submit event: the listener cancels it.
    auto result = pipeline.dispatch_submit(submission->form_element);
    EXPECT_TRUE(result.default_prevented);
    EXPECT_TRUE(result.mutated);  // the handler also set an attribute
}

TEST(DocumentPipelineTest, TodoDemoAssetsDriveTheFullFlow) {
    // End-to-end check of the shipped interactive demo (assets/stub/pages/todo.*):
    // the external script builds the seeded list, and Enter adds a new todo.
    const auto read_file = [](const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    };
    const std::string html = read_file("assets/stub/pages/todo.html");
    const std::string js = read_file("assets/stub/pages/todo.js");
    ASSERT_FALSE(html.empty()) << "todo.html not found (run tests from the repo root)";
    ASSERT_FALSE(js.empty()) << "todo.js not found";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 800, 600};
    const std::string base = "https://example.dev/todo";

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, base);
    // Provide the external todo.js body the way the resource pipeline would.
    pipeline.run_scripts([&](std::string_view src) -> std::optional<std::string_view> {
        if (src == "assets/stub/pages/todo.js") return std::string_view(js);
        return std::nullopt;
    });

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, base);
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    // The external script seeded two todos via createElement/appendChild.
    EXPECT_TRUE(painted_has("Tick the checkbox to complete a task"));
    EXPECT_TRUE(painted_has("This one starts completed"));

    // The input autofocuses; type a task and press Enter to add it.
    ASSERT_TRUE(pipeline.focus_autofocus_input());
    pipeline.handle_text_input("Buy milk");

    Hummingbird::InputEvent enter{};
    enter.type = Hummingbird::EventType::KeyDown;
    enter.key.key = Hummingbird::Key::Enter;
    auto result = pipeline.handle_key_down(enter, base);
    EXPECT_TRUE(result.mutated);  // the keydown listener added a todo

    EXPECT_TRUE(painted_has("Buy milk"));

    // The hash-routed filter: switching to #/active hides the completed seed and
    // keeps the active tasks (7.2.5 hashchange re-filters in place).
    pipeline.set_location(base);
    auto frag = pipeline.navigate_fragment(base + "#/active");
    EXPECT_TRUE(frag.hash_changed);
    EXPECT_TRUE(painted_has("Buy milk"));                    // active → shown
    EXPECT_FALSE(painted_has("This one starts completed"));  // completed → hidden
}

TEST(DocumentPipelineTest, JsFocusMakesInputTheCaretTarget) {
    // element.focus() from JS makes an editable input the live caret target, so
    // subsequent typing lands in it (7.2.6).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } </style></head>
  <body>
    <input id="field">
    <script> document.getElementById('field').focus(); </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();  // runs the inline focus() call

    // The JS focus() routed to the input controller: the field is the caret target.
    EXPECT_TRUE(pipeline.has_focused_input());
    pipeline.handle_text_input("hello");
    auto value = pipeline.focused_input_value();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "hello");
}

TEST(DocumentPipelineTest, JsFocusBlurDispatchFocusEvents) {
    // element.focus()/blur() from JS fire focus/blur (and change when the value
    // was edited), matching user-driven transitions (T-FOCUS-EVENTS-FROM-JS-1,
    // 7.7.2). The blur is triggered from a click handler, so it dispatches
    // re-entrantly while the click dispatch is still on the stack (7.7.1).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } input, p { display: block; } </style></head>
  <body>
    <input id="in" value="">
    <p id="log">start</p>
    <script>
      var el = document.getElementById('in');
      var log = document.getElementById('log');
      el.addEventListener('focus', function() { log.textContent = 'focus'; });
      el.addEventListener('input', function(e) { log.textContent = 'input:' + e.target.value; });
      el.addEventListener('change', function() { log.textContent = 'change'; });
      el.addEventListener('blur', function() { log.textContent = log.textContent + '+blur'; });
      document.addEventListener('click', function() { el.blur(); });
      el.focus();
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();  // registers listeners and calls el.focus()

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    // The inline el.focus() made the field the caret target and fired `focus`.
    EXPECT_TRUE(pipeline.has_focused_input());
    EXPECT_TRUE(painted_has("focus"));

    // Type into the (JS-)focused field: `input` fires with the new value.
    pipeline.handle_text_input("hi");
    EXPECT_TRUE(painted_has("input:hi"));

    // A click runs the document listener, which calls el.blur() while the click
    // dispatch is on the stack. The edited field fires change then blur.
    DocumentPipeline::HitTestContext click{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 1};
    pipeline.dispatch_click(click);
    EXPECT_FALSE(pipeline.has_focused_input());
    EXPECT_TRUE(painted_has("change+blur"));
}

TEST(DocumentPipelineTest, FragmentNavigationFiresHashchangeWithoutTeardown) {
    // navigate_fragment fires hashchange in place; the listener (and DOM) survive,
    // so a second fragment change fires again (7.2.5).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } p { display: block; } </style></head>
  <body>
    <p id="status">start</p>
    <script>
      window.addEventListener('hashchange', function() {
        document.getElementById('status').textContent = 'route:' + location.hash;
      });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};
    const std::string base = "https://example.dev/todo";

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, base);
    pipeline.set_location(base);
    pipeline.run_scripts();  // registers the hashchange listener

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, base);
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    auto r1 = pipeline.navigate_fragment(base + "#/active");
    EXPECT_TRUE(r1.hash_changed);
    EXPECT_TRUE(r1.mutated);
    EXPECT_TRUE(painted_has("route:#/active"));

    // The listener survived (no document teardown): a second change fires again.
    auto r2 = pipeline.navigate_fragment(base + "#/completed");
    EXPECT_TRUE(r2.hash_changed);
    EXPECT_TRUE(painted_has("route:#/completed"));

    // Navigating to the same fragment reports no change.
    auto r3 = pipeline.navigate_fragment(base + "#/completed");
    EXPECT_FALSE(r3.hash_changed);
}

TEST(DocumentPipelineTest, CheckboxClickTogglesAndFiresChange) {
    // Clicking a checkbox toggles its checkedness (default action) and fires a
    // change event whose handler sees the new value (7.2.6).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } p { display: block; } </style></head>
  <body>
    <input type="checkbox" id="cb">
    <p id="status">start</p>
    <script>
      document.getElementById('cb').addEventListener('change', function(e) {
        document.getElementById('status').textContent = 'changed:' + e.target.checked;
      });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    // Click the checkbox (top-left 13x13 box): toggles on, change fires true.
    DocumentPipeline::HitTestContext click{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 1};
    auto r1 = pipeline.dispatch_click(click);
    EXPECT_TRUE(r1.mutated);
    EXPECT_TRUE(painted_has("changed:true"));

    // Clicking again toggles back off.
    auto r2 = pipeline.dispatch_click(click);
    EXPECT_TRUE(r2.mutated);
    EXPECT_TRUE(painted_has("changed:false"));
}

TEST(DocumentPipelineTest, InnerHtmlInChangeHandlerDoesNotCorruptDispatch) {
    // Regression: a checkbox `change` handler that rebuilds an ancestor via
    // innerHTML destroys the event target's own subtree mid-dispatch. The click's
    // trailing hit-test (and the stale render tree) must not then dereference a
    // freed node. innerHTML detaches the old subtree (keeps it alive) rather than
    // destroying it, so the dispatch completes and the container rebuilds cleanly.
    // This is the hazard the pinned TodoMVC harness surfaced (render() re-emits the
    // list on every change).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } </style></head>
  <body>
    <div id="box"><input type="checkbox" id="cb"></div>
    <p id="status">start</p>
    <script>
      document.getElementById('cb').addEventListener('change', function() {
        document.getElementById('status').textContent = 'changed';
        document.getElementById('box').innerHTML = '<span>rebuilt</span>';
      });
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
    pipeline.run_scripts();

    const auto painted_has = [&](const char* text) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");
        pipeline.paint(graphics, {viewport, false, 0.0f});
        return std::find(graphics.drawn_texts.begin(), graphics.drawn_texts.end(), text) != graphics.drawn_texts.end();
    };

    // Click the checkbox (top-left 13x13 box): change fires, the handler wipes and
    // rebuilds #box while #cb (the event target) is inside it.
    DocumentPipeline::HitTestContext click{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f, 1};
    auto r = pipeline.dispatch_click(click);
    EXPECT_TRUE(r.mutated);
    EXPECT_TRUE(painted_has("changed"));  // status updated (no crash)
    EXPECT_TRUE(painted_has("rebuilt"));  // #box rebuilt from innerHTML
}

TEST(DocumentPipelineTest, HackerNewsStyleCommentCollapse) {
    // Secondary proof (M7): reproduces Hacker News' hn.js comment-collapse exactly
    // — a document-delegated click handler that does `new URL(el.href, location)`
    // (needs the URL polyfill), routes a `.togg` click to a class-based collapse
    // (add `coll`, hide the `.comment` + child rows via `.noshow`, flip the toggle
    // label), then calls preventDefault. This exercises the two fixes that make
    // the real page work: the URL polyfill (so the handler doesn't throw before
    // collapsing) and preventDefault reporting (so the `javascript:void(0)` href
    // doesn't navigate).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style>
    body { margin: 0; padding: 0; }
    .noshow { display: none; }
    .nosee { visibility: hidden; }
  </style></head>
  <body>
    <table><tbody>
      <tr class="athing comtr" id="c1"><td>
        <span class="ind" indent="0"></span>
        <a class="togg clicky" id="c1" n="1" href="javascript:void(0)">[-]</a>
        <div class="comment">Parent comment body</div>
      </td></tr>
      <tr class="athing comtr" id="c2"><td>
        <span class="ind" indent="1"></span>
        <a class="togg clicky" id="c2" n="1" href="javascript:void(0)">[-]</a>
        <div class="comment">Child comment body</div>
      </td></tr>
    </tbody></table>
    <script>
      function $(id){return document.getElementById(id);}
      function byClass(el,cl){return el?el.getElementsByClassName(cl):[];}
      function hasClass(el,cl){return el&&el.className?((' '+el.className+' ').indexOf(' '+cl+' ')>=0):false;}
      function addClass(el,cl){if(el&&!hasClass(el,cl))el.className=((el.className||'')+' '+cl).replace(/^ /,'');}
      function remClass(el,cl){if(el)el.className=(' '+(el.className||'')+' ').split(' '+cl+' ').join(' ').replace(/^ +| +$/g,'');}
      function upclass(el,cl){while(el){if(el.getAttribute&&hasClass(el,cl))return el;el=el.parentNode;}return null;}
      function vis(el,on){if(el){(on?remClass:addClass)(el,'nosee');}}
      function setshow(el,on){(on?remClass:addClass)(el,'noshow');}
      function nextcomm(el){while(el=el.nextElementSibling){if(hasClass(el,'comtr'))return el;}return null;}
      function ind(tr){var e=byClass(tr,'ind')[0];return e?parseInt(e.getAttribute('indent'),10):0;}
      function collstate(tr,coll){
        (coll?addClass:remClass)(tr,'coll');
        vis(byClass(tr,'votelinks')[0],!coll);
        setshow(byClass(tr,'comment')[0],!coll);
        var el=byClass(tr,'togg')[0];
        el.innerHTML=coll?('['+el.getAttribute('n')+' more]'):'[-]';
      }
      function kids(tr,show){var n=ind(tr);while((tr=nextcomm(tr))&&ind(tr)>n){setshow(tr,show);}}
      function toggleCollapse(id){var tr=$(id),coll=!hasClass(tr,'coll');collstate(tr,coll);kids(tr,!coll);}
      function onclick(ev){
        var el=upclass(ev.target,'clicky');
        if(el){
          var u=new URL(el.href,location);
          if(u.pathname=='/vote'){}
          else if(hasClass(el,'togg')){toggleCollapse(el.getAttribute('id'));}
          ev.preventDefault();
        }
      }
      document.addEventListener('click',onclick);
    </script>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    const Rect viewport{0, 0, 800, 600};
    const std::string base = "https://news.ycombinator.com/item?id=1";

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.set_location(base);
    pipeline.apply_styles_and_layout(graphics, viewport, base);
    pipeline.run_scripts();

    const auto painted = [&](const char* needle) {
        graphics.drawn_texts.clear();
        pipeline.apply_styles_and_layout(graphics, viewport, base);
        pipeline.paint(graphics, {viewport, false, 0.0f});
        std::string joined;
        for (const auto& t : graphics.drawn_texts) joined += t;
        return joined.find(needle) != std::string::npos;
    };

    // Before collapse: both comments visible.
    EXPECT_TRUE(painted("Parent comment body"));
    EXPECT_TRUE(painted("Child comment body"));

    // Click the parent's [-] toggle.
    pipeline.apply_styles_and_layout(graphics, viewport, base);
    auto toggle = center_by_class(pipeline.render_root(), "togg");
    ASSERT_TRUE(toggle.has_value()) << "collapse toggle not found";
    auto result = pipeline.dispatch_click({*toggle, viewport, base, 0.0f, 1});

    // The handler ran the collapse and called preventDefault, so the bogus
    // javascript:void(0) navigation is suppressed.
    EXPECT_TRUE(result.mutated);
    EXPECT_TRUE(result.default_prevented)
        << "preventDefault not reported — the page would navigate to javascript:void(0)";

    // After collapse: the parent's body and the child row are hidden; the toggle
    // label flips to the collapsed count.
    EXPECT_FALSE(painted("Parent comment body"));
    EXPECT_FALSE(painted("Child comment body"));
    EXPECT_TRUE(painted("1 more"));
}

TEST(DocumentPipelineTest, CollectsBackgroundImageLinksFromStyles) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      div { background-image: url(/img/background.png); }
    </style>
  </head>
  <body>
    <div>Box</div>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    const auto& links = pipeline.background_image_links();
    ASSERT_EQ(links.size(), 1u);
    EXPECT_EQ(links[0], "/img/background.png");
}

TEST(DocumentPipelineTest, PaintsBackgroundImagesFromResources) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      div { width: 10px; height: 10px; background-image: url(/img/background.png); }
    </style>
  </head>
  <body>
    <div>Box</div>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    ImageBitmap bitmap;
    bitmap.width = 1;
    bitmap.height = 1;
    bitmap.stride = 4;
    bitmap.format = PixelFormat::BGRA32;
    bitmap.pixels = {0, 0, 0, 255};
    const std::string image_url = "https://example.dev/img/background.png";
    ASSERT_TRUE(store.begin_request(image_url, ResourceType::Image));
    ASSERT_TRUE(store.mark_ready(image_url, ResourceType::Image, {}));
    ASSERT_TRUE(store.set_image(image_url, ResourceType::Image, std::move(bitmap)));

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    DocumentPipeline::PaintContext context{viewport, false, 0.0f};
    pipeline.paint(graphics, context);

    EXPECT_GT(graphics.image_calls, 0);
}

TEST(DocumentPipelineTest, PaintsFocusedInputTextWhenContentBoxCollapses) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      input { width: 280px; height: 10px; padding: 5px 7px; border: 1px solid #666; }
    </style>
  </head>
  <body>
    <input id="q" type="text" />
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 400, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    DocumentPipeline::HitTestContext hit{Point{12.0f, 12.0f}, viewport, "https://example.dev", 0.0f};
    ASSERT_TRUE(pipeline.focus_input_at(hit));

    auto edit = pipeline.handle_text_input("hello");
    ASSERT_TRUE(edit.handled);

    DocumentPipeline::PaintContext paint{viewport, false, 0.0f};
    pipeline.paint(graphics, paint);

    bool saw_typed_text = false;
    for (const auto& text : graphics.drawn_texts) {
        if (text == "hello") {
            saw_typed_text = true;
            break;
        }
    }
    EXPECT_TRUE(saw_typed_text);
}

TEST(DocumentPipelineTest, ContentHeightIncludesDescendantsBeyondRootHeightClamp) {
    std::string rows;
    rows.reserve(2048);
    for (int i = 0; i < 80; ++i) {
        rows += "<div>row</div>";
    }

    const std::string html =
        "<!doctype html><html><head><style>"
        "body{margin:0;padding:0;max-height:50%;}"
        "div{margin:0;padding:0;}"
        "</style></head><body>" +
        rows + "</body></html>";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect viewport{0, 0, 800, 600};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    EXPECT_GT(pipeline.content_height(), 300.0f);
}

namespace {
using Hummingbird::Engine::DocumentModel;
using Hummingbird::Engine::DocumentScripting;

// Concatenated text of the element with the given id, or empty when absent.
std::string element_text_by_id(Hummingbird::DOM::Node* root, std::string_view id) {
    using Hummingbird::DOM::Element;
    using Hummingbird::DOM::Node;
    using Hummingbird::DOM::Text;

    std::function<Element*(Node*)> find = [&](Node* node) -> Element* {
        if (auto* element = dynamic_cast<Element*>(node)) {
            const auto* attr = element->find_attribute("id");
            if (attr && *attr == id) return element;
        }
        for (const auto& child : node->get_children()) {
            if (auto* found = find(child.get())) return found;
        }
        return nullptr;
    };
    Element* element = root ? find(root) : nullptr;
    if (!element) return {};

    std::string text;
    std::function<void(Node*)> collect = [&](Node* node) {
        if (auto* text_node = dynamic_cast<Text*>(node)) {
            text += text_node->get_text();
        }
        for (const auto& child : node->get_children()) collect(child.get());
    };
    collect(element);
    return text;
}
}  // namespace

TEST(DocumentScriptingTest, RunsInlineAndExternalScriptsInDocumentOrder) {
    // Three <script>s — inline, external, inline — each overwriting the same
    // element; the surviving value proves document-order execution (7.0.1).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p id="out">initial</p>
    <script>document.getElementById('out').textContent = 'first-inline';</script>
    <script src="app.js"></script>
    <script>document.getElementById('out').textContent = 'second-inline';</script>
  </body>
</html>
)HTML";

    DocumentModel model;
    ASSERT_TRUE(model.parse_html(html).ok);
    ASSERT_EQ(model.document_scripts().size(), 3u);
    EXPECT_FALSE(model.document_scripts()[0].is_external());
    EXPECT_TRUE(model.document_scripts()[1].is_external());
    EXPECT_EQ(model.document_scripts()[1].src, "app.js");

    std::vector<std::string> looked_up;
    DocumentScripting scripting(Hummingbird::create_script_engine());
    const bool mutated = scripting.run_document_scripts(model, [&](std::string_view src) {
        looked_up.emplace_back(src);
        return std::optional<std::string_view>("document.getElementById('out').textContent = 'external';");
    });

    EXPECT_TRUE(mutated);
    ASSERT_EQ(looked_up.size(), 1u);
    EXPECT_EQ(looked_up[0], "app.js");
    // The last script in document order wins; if the external script had run
    // first or last the value would differ.
    EXPECT_EQ(element_text_by_id(model.dom_root(), "out"), "second-inline");
}

TEST(DocumentScriptingTest, ExternalScriptRunsBetweenInlineScripts) {
    const std::string html = R"HTML(
<html><body>
  <p id="out">initial</p>
  <script>document.getElementById('out').textContent = 'inline';</script>
  <script src="late.js"></script>
</body></html>
)HTML";

    DocumentModel model;
    ASSERT_TRUE(model.parse_html(html).ok);

    DocumentScripting scripting(Hummingbird::create_script_engine());
    (void)scripting.run_document_scripts(model, [&](std::string_view) {
        return std::optional<std::string_view>("document.getElementById('out').textContent = 'external';");
    });

    // The external script is last in document order, so its write survives.
    EXPECT_EQ(element_text_by_id(model.dom_root(), "out"), "external");
}

TEST(DocumentScriptingTest, MissingExternalScriptIsSkippedButInlineStillRuns) {
    // Fail-soft: a script whose fetch failed is skipped with a warning; the
    // rest of the document's scripts must still execute.
    const std::string html = R"HTML(
<html><body>
  <p id="out">initial</p>
  <script src="gone.js"></script>
  <script>document.getElementById('out').textContent = 'inline-ran';</script>
</body></html>
)HTML";

    DocumentModel model;
    ASSERT_TRUE(model.parse_html(html).ok);

    DocumentScripting scripting(Hummingbird::create_script_engine());
    const bool mutated =
        scripting.run_document_scripts(model, [&](std::string_view) { return std::optional<std::string_view>{}; });

    EXPECT_TRUE(mutated);
    EXPECT_EQ(element_text_by_id(model.dom_root(), "out"), "inline-ran");
}

TEST(DocumentModelTest, ScriptCollectionFiltersNonJsTypesAndPrefersSrc) {
    const std::string html = R"HTML(
<html><body>
  <script type="application/json">{"data": 1}</script>
  <script type="module">import x from 'y';</script>
  <script type="text/javascript" src="a.js">ignored inline fallback</script>
  <script>var inline1 = true;</script>
</body></html>
)HTML";

    DocumentModel model;
    ASSERT_TRUE(model.parse_html(html).ok);

    const auto& scripts = model.document_scripts();
    ASSERT_EQ(scripts.size(), 2u);
    // JSON data block and module are skipped; src wins over inline body.
    EXPECT_TRUE(scripts[0].is_external());
    EXPECT_EQ(scripts[0].src, "a.js");
    EXPECT_TRUE(scripts[0].text.empty());
    EXPECT_FALSE(scripts[1].is_external());
    EXPECT_NE(scripts[1].text.find("inline1"), std::string::npos);
}
