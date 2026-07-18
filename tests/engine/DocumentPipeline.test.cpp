#include "engine/document/DocumentPipeline.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <optional>
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
