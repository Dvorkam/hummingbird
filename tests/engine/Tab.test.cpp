#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "core/platform_api/InputEvent.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "test_utils/HeadlessTabHarness.h"
#include "test_utils/TestFakes.h"

namespace {
using Hummingbird::NetworkResponse;
using Hummingbird::Test::DeferredNetwork;
using Hummingbird::Test::HeadlessTabHarness;
using Hummingbird::Test::InlineImageDecoder;
using Hummingbird::Test::InlineNetwork;
using Hummingbird::Test::RoutingNetwork;

class TlsFailureNetwork final : public Hummingbird::INetwork {
public:
    explicit TlsFailureNetwork(std::string body) : body_(std::move(body)) {}

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const Hummingbird::NetworkRequestOptions& options = {}) override {
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        last_allow_insecure_ = options.allow_insecure;
        if (options.allow_insecure) {
            response.status = 200;
            response.body = body_;
        } else {
            response.error = Hummingbird::NetworkError::TlsVerificationFailed;
        }
        if (callback) callback(std::move(response));
    }

    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const Hummingbird::NetworkRequestOptions& options = {}) override {
        (void)body;
        get(url, std::move(callback), options);
    }

    void shutdown() override {}

    bool last_allow_insecure() const { return last_allow_insecure_; }

private:
    std::string body_;
    bool last_allow_insecure_ = false;
};
}  // namespace

TEST(EngineTabTest, NavigateAndBuildsDocument) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Tab Test</title>
    <style>
      body { margin: 10px; }
      p { color: #333; }
    </style>
  </head>
  <body>
    <p>Hello from Tab</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);

    harness.navigate("https://example.dev");

    EXPECT_TRUE(harness.tick());
    harness.paint(false);

    EXPECT_FALSE(harness.tick());
    EXPECT_EQ(harness.tab().requested_url(), "https://example.dev");
}

TEST(EngineTabTest, FetchesLinkedStylesheet) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="styles/main.css">
  </head>
  <body>
    <p>Styled text</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test", html);
    network->set_response("https://acme.test/styles/main.css", "p { color: #cc0000; }");

    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);

    harness.navigate("https://acme.test");
    harness.tick();
    harness.tick();

    auto view =
        harness.resource_view("https://acme.test/styles/main.css", Hummingbird::Engine::ResourceType::Stylesheet);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_EQ(view->body, "p { color: #cc0000; }");

    EXPECT_NE(std::find(network_ptr->requested().begin(), network_ptr->requested().end(),
                        "https://acme.test/styles/main.css"),
              network_ptr->requested().end());
}

TEST(EngineTabTest, FetchesLinkedImage) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <img SRC="images/logo.png" alt="logo">
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test", html);
    network->set_response("https://acme.test/images/logo.png", "PNGDATA");

    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider),
                               std::make_unique<InlineImageDecoder>());

    harness.navigate("https://acme.test");
    harness.tick();
    harness.tick();

    auto view = harness.resource_view("https://acme.test/images/logo.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_EQ(view->body, "PNGDATA");
    ASSERT_NE(view->image, nullptr);
    EXPECT_EQ(view->image->width, 2);
    EXPECT_EQ(view->image->height, 2);

    EXPECT_NE(std::find(network_ptr->requested().begin(), network_ptr->requested().end(),
                        "https://acme.test/images/logo.png"),
              network_ptr->requested().end());
}

TEST(EngineTabTest, RebuildsWhenStylesheetArrives) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="styles/main.css">
  </head>
  <body>
    <p>Styled text</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<DeferredNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test", html);
    network->defer_response("https://acme.test/styles/main.css", "p { color: blue; }");

    auto fallback = std::make_unique<DeferredNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);

    harness.navigate("https://acme.test");
    EXPECT_TRUE(harness.tick());
    EXPECT_FALSE(harness.tick());

    auto view =
        harness.resource_view("https://acme.test/styles/main.css", Hummingbird::Engine::ResourceType::Stylesheet);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Loading);

    network_ptr->complete("https://acme.test/styles/main.css");
    EXPECT_TRUE(harness.tick());
}

TEST(EngineTabTest, ExtensionCssInjectionUpdatesStylePipeline) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p>Styled by extension</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://acme.test", html);
    network->set_response("https://acme.test/ext.png", "PNGDATA");
    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider),
                               std::make_unique<InlineImageDecoder>());

    harness.navigate("https://acme.test");
    ASSERT_TRUE(harness.tick());

    EXPECT_TRUE(harness.tab().insert_extension_css("body { background-image: url('/ext.png'); }"));
    EXPECT_TRUE(harness.tick());
    EXPECT_TRUE(harness.tick());

    auto view = harness.resource_view("https://acme.test/ext.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
}

TEST(EngineTabTest, ExtensionCssInsertedBeforeNavigateAppliesOnDocumentBuild) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p>Styled by extension</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://acme.test", html);
    network->set_response("https://acme.test/ext-before-nav.png", "PNGDATA");
    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider),
                               std::make_unique<InlineImageDecoder>());

    EXPECT_TRUE(harness.tab().insert_extension_css("body { background-image: url('/ext-before-nav.png'); }"));
    harness.navigate("https://acme.test");
    EXPECT_TRUE(harness.tick());
    EXPECT_TRUE(harness.tick());

    auto view = harness.resource_view("https://acme.test/ext-before-nav.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
}

TEST(EngineTabTest, ExtensionCssInjectionInvalidatesOnceWithoutResourceFollowups) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p>Styled by extension</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://acme.test", html);
    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);

    harness.navigate("https://acme.test");
    ASSERT_TRUE(harness.tick());

    EXPECT_TRUE(harness.tab().insert_extension_css("p { color: #111111; }"));
    EXPECT_TRUE(harness.tick());
    EXPECT_FALSE(harness.tick());
}

TEST(EngineTabTest, ExtensionCssInjectedBeforeNavigationDoesNotDoubleRebuildOnCommit) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p>Commit budget test</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://acme.test", html);
    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);

    EXPECT_TRUE(harness.tab().insert_extension_css("p { color: #222222; }"));
    harness.navigate("https://acme.test");
    EXPECT_TRUE(harness.tick());
    EXPECT_FALSE(harness.tick());
}

TEST(EngineTabTest, DuplicateExtensionCssDoesNotTriggerExtraInvalidation) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p>Duplicate CSS budget test</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://acme.test", html);
    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);

    harness.navigate("https://acme.test");
    ASSERT_TRUE(harness.tick());

    EXPECT_TRUE(harness.tab().insert_extension_css("p { color: #333333; }"));
    EXPECT_TRUE(harness.tick());
    EXPECT_FALSE(harness.tick());

    EXPECT_TRUE(harness.tab().insert_extension_css("p { color: #333333; }"));
    EXPECT_FALSE(harness.tick());
}

TEST(EngineTabTest, NavigationCommitUrlIsExposedOncePerCommittedDocument) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p>Commit test</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<DeferredNetwork>();
    auto* network_ptr = network.get();
    network->defer_response("https://acme.test", html);
    auto fallback = std::make_unique<DeferredNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);

    harness.navigate("https://acme.test");
    EXPECT_TRUE(harness.tick());
    EXPECT_FALSE(harness.tab().consume_navigation_commit_url().has_value());
    EXPECT_FALSE(harness.tick());
    EXPECT_FALSE(harness.tab().consume_navigation_commit_url().has_value());

    network_ptr->complete("https://acme.test");
    EXPECT_TRUE(harness.tick());

    auto first = harness.tab().consume_navigation_commit_url();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, "https://acme.test");
    EXPECT_FALSE(harness.tab().consume_navigation_commit_url().has_value());
}

TEST(EngineTabTest, HitTestResolvesLink) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <a HREF="/next">Next</a>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 200, 200});

    harness.navigate("https://example.dev");
    EXPECT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{10.0f, 12.0f};
    auto link = harness.tab().hit_test_link(point, harness.viewport());
    ASSERT_TRUE(link.has_value());
    EXPECT_EQ(*link, "https://example.dev/next");
}

TEST(EngineTabTest, InspectAtDescribesElementUnderPoint) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style>body { margin: 0; } #box { display: block; width: 100px; height: 40px; margin: 10px; padding: 5px; }</style></head>
  <body>
    <div id="box" class="panel">hi</div>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 200, 200});
    harness.navigate("https://example.dev");
    EXPECT_TRUE(harness.tick());

    // A point inside the #box border area (origin ~10,10; 110x50 border box).
    auto info = harness.tab().inspect_at({30.0f, 30.0f}, harness.viewport());
    ASSERT_TRUE(info.has_value());
    EXPECT_NE(info->find("<div>"), std::string::npos);
    EXPECT_NE(info->find("#box"), std::string::npos);
    EXPECT_NE(info->find("panel"), std::string::npos);
    EXPECT_NE(info->find("display=block"), std::string::npos);
    EXPECT_NE(info->find("rect="), std::string::npos);

    // A point outside any element returns nothing.
    EXPECT_FALSE(harness.tab().inspect_at({190.0f, 190.0f}, harness.viewport()).has_value());
}

TEST(EngineTabTest, HitTestSkipsPointerEventsNone) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style>a { pointer-events: none; }</style></head>
  <body>
    <a HREF="/next">Next</a>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 200, 200});
    harness.navigate("https://example.dev");
    EXPECT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{10.0f, 12.0f};
    // The anchor is transparent to hit-testing, so the click falls through.
    EXPECT_FALSE(harness.tab().hit_test_link(point, harness.viewport()).has_value());
}

TEST(EngineTabTest, HitTestSkipsVisibilityHidden) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style>a { visibility: hidden; }</style></head>
  <body>
    <a HREF="/next">Next</a>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 200, 200});
    harness.navigate("https://example.dev");
    EXPECT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{10.0f, 12.0f};
    EXPECT_FALSE(harness.tab().hit_test_link(point, harness.viewport()).has_value());
}

TEST(EngineTabTest, HitTestSkipsEmptyClipRect) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head><style>a { position: absolute; top: 0; left: 0; clip: rect(0 0 0 0); }</style></head>
  <body>
    <a HREF="/next">Next</a>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 200, 200});
    harness.navigate("https://example.dev");
    EXPECT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{4.0f, 6.0f};
    // The clipped-away anchor is hidden from hit-testing.
    EXPECT_FALSE(harness.tab().hit_test_link(point, harness.viewport()).has_value());
}

TEST(EngineTabTest, FocusesInputAndEditsValue) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <input value="hi">
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    EXPECT_TRUE(harness.tab().focus_input_at(point, harness.viewport()));
    EXPECT_TRUE(harness.tab().has_focused_input());

    EXPECT_TRUE(harness.tab().handle_text_input("!"));
    auto value = harness.tab().focused_input_value();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "hi!");

    Hummingbird::InputEvent backspace_event;
    backspace_event.type = Hummingbird::EventType::KeyDown;
    backspace_event.key.key = Hummingbird::Key::Backspace;
    auto backspace_result = harness.tab().handle_key_down(backspace_event);
    EXPECT_TRUE(backspace_result.handled);
    value = harness.tab().focused_input_value();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "hi");
}

TEST(EngineTabTest, FocusPrefersTextInputOverOverlappingSubmitInput) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form>
      <input id="search" type="text" value="">
      <input id="submit" type="submit" value="Search">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    ASSERT_TRUE(harness.tab().focus_input_at(point, harness.viewport()));
    ASSERT_TRUE(harness.tab().handle_text_input("duck"));

    auto value = harness.tab().focused_input_value();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "duck");
}

TEST(EngineTabTest, AutofocusInputIsFocusedAfterDocumentLoad) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form>
      <input id="search" name="q" autofocus>
      <input id="submit" type="submit" value="Search">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    EXPECT_TRUE(harness.tab().has_focused_input());
    EXPECT_TRUE(harness.tab().handle_text_input("hello"));
    auto value = harness.tab().focused_input_value();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "hello");
}

TEST(EngineTabTest, SubmitsFocusedFormAsGet) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form action="/search" method="GET">
      <input name="q">
      <input name="lang" value="en">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    EXPECT_TRUE(harness.tab().focus_input_at(point, harness.viewport()));
    EXPECT_TRUE(harness.tab().handle_text_input("hello world"));

    Hummingbird::InputEvent enter_event;
    enter_event.type = Hummingbird::EventType::KeyDown;
    enter_event.key.key = Hummingbird::Key::Enter;
    auto result = harness.tab().handle_key_down(enter_event);
    EXPECT_TRUE(result.handled);
    ASSERT_TRUE(result.submitted_form.has_value());
    EXPECT_EQ(result.submitted_form->url, "https://example.dev/search?q=hello+world&lang=en");
    EXPECT_EQ(result.submitted_form->method, Hummingbird::Engine::FormSubmitMethod::Get);
}

TEST(EngineTabTest, FormWithEmptyMethodSubmitsAsGet) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form action="/search" method="">
      <input name="q" value="test">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    EXPECT_TRUE(harness.tab().focus_input_at(point, harness.viewport()));

    Hummingbird::InputEvent enter_event;
    enter_event.type = Hummingbird::EventType::KeyDown;
    enter_event.key.key = Hummingbird::Key::Enter;
    auto result = harness.tab().handle_key_down(enter_event);
    EXPECT_TRUE(result.handled);
    ASSERT_TRUE(result.submitted_form.has_value());
    EXPECT_EQ(result.submitted_form->url, "https://example.dev/search?q=test");
    EXPECT_EQ(result.submitted_form->method, Hummingbird::Engine::FormSubmitMethod::Get);
}

TEST(EngineTabTest, SubmitsInputSubmitControlByClick) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form action="/search" method="get">
      <input id="submit" type="submit" value="Search">
      <input name="q" value="saturn">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    auto submitted = harness.tab().submit_form_at(point, harness.viewport());
    ASSERT_TRUE(submitted.has_value());
    EXPECT_EQ(submitted->url, "https://example.dev/search?q=saturn");
    EXPECT_EQ(submitted->method, Hummingbird::Engine::FormSubmitMethod::Get);
}

// DDG-shaped regression: an absolutely-positioned submit button must only
// capture clicks inside its own box. Nodes kept in the hit-test walk to reach
// absolute descendants must not resolve for points outside themselves.
TEST(EngineTabTest, AbsoluteSubmitButtonDoesNotCaptureClicksOutsideItsBox) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      form { position: relative; width: 280px; height: 40px; }
      #submit { position: absolute; top: 0; right: 0; width: 40px; height: 40px; }
    </style>
  </head>
  <body>
    <form action="/search" method="get">
      <input name="q" value="saturn">
      <input id="submit" type="submit" value="S">
    </form>
    <p>plain page text far below the form</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    // Clicking empty space well below the form must not submit anything.
    Hummingbird::Layout::Point outside{150.0f, 180.0f};
    EXPECT_FALSE(harness.tab().submit_form_at(outside, harness.viewport()).has_value());

    // Clicking the absolute button itself still submits.
    Hummingbird::Layout::Point on_button{260.0f, 20.0f};
    auto submitted = harness.tab().submit_form_at(on_button, harness.viewport());
    ASSERT_TRUE(submitted.has_value());
    EXPECT_EQ(submitted->url, "https://example.dev/search?q=saturn");
}

TEST(EngineTabTest, SubmitsInputSubmitControlWithFormAttribute) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <input id="submit" type="submit" form="search-form" value="Search">
    <form id="search-form" action="/search" method="get">
      <input name="q" value="venus">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    auto submitted = harness.tab().submit_form_at(point, harness.viewport());
    ASSERT_TRUE(submitted.has_value());
    EXPECT_EQ(submitted->url, "https://example.dev/search?q=venus");
    EXPECT_EQ(submitted->method, Hummingbird::Engine::FormSubmitMethod::Get);
}

TEST(EngineTabTest, EnterSubmitsFormWithInputSubmitControl) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form action="/search" method="get">
      <input name="q">
      <input type="submit" value="Search">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    EXPECT_TRUE(harness.tab().focus_input_at(point, harness.viewport()));
    EXPECT_TRUE(harness.tab().handle_text_input("pluto"));

    Hummingbird::InputEvent enter_event;
    enter_event.type = Hummingbird::EventType::KeyDown;
    enter_event.key.key = Hummingbird::Key::Enter;
    auto result = harness.tab().handle_key_down(enter_event);
    EXPECT_TRUE(result.handled);
    ASSERT_TRUE(result.submitted_form.has_value());
    EXPECT_EQ(result.submitted_form->url, "https://example.dev/search?q=pluto");
    EXPECT_EQ(result.submitted_form->method, Hummingbird::Engine::FormSubmitMethod::Get);
}

TEST(EngineTabTest, EnterSubmitsPostFormWithEncodedBody) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form action="/html/" method="post">
      <input name="q">
      <input type="submit" value="Search">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    EXPECT_TRUE(harness.tab().focus_input_at(point, harness.viewport()));
    EXPECT_TRUE(harness.tab().handle_text_input("duck duck go"));

    Hummingbird::InputEvent enter_event;
    enter_event.type = Hummingbird::EventType::KeyDown;
    enter_event.key.key = Hummingbird::Key::Enter;
    auto result = harness.tab().handle_key_down(enter_event);
    EXPECT_TRUE(result.handled);
    ASSERT_TRUE(result.submitted_form.has_value());
    EXPECT_EQ(result.submitted_form->url, "https://example.dev/html/");
    EXPECT_EQ(result.submitted_form->method, Hummingbird::Engine::FormSubmitMethod::Post);
    EXPECT_EQ(result.submitted_form->body, "q=duck+duck+go");
    EXPECT_EQ(result.submitted_form->content_type, "application/x-www-form-urlencoded");
}

// End-to-end regression on the pinned DuckDuckGo HTML homepage snapshot
// (tests/fixtures/ddg): drive the real focus -> type -> submit -> navigate flow
// through the tab. Guards the exact DDG form contract (POST to /html/, the
// autofocused name="q" search box) so a regression in autofocus, text editing,
// or form submission fails CI.
TEST(EngineTabTest, DdgHomepageSnapshotFocusTypeSubmitFlow) {
    std::ifstream file(std::string(HB_TEST_FIXTURE_DIR) + "/ddg/ddg_home.html", std::ios::binary);
    ASSERT_TRUE(file) << "missing fixture ddg/ddg_home.html";
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string html = buffer.str();
    ASSERT_FALSE(html.empty());

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 1024, 768});
    harness.navigate("https://html.duckduckgo.com/html/");
    ASSERT_TRUE(harness.tick());

    // The search box carries `autofocus`, so typing lands there with no click.
    ASSERT_TRUE(harness.tab().has_focused_input());
    EXPECT_TRUE(harness.tab().handle_text_input("hummingbird browser"));
    auto typed = harness.tab().focused_input_value();
    ASSERT_TRUE(typed.has_value());
    EXPECT_EQ(*typed, "hummingbird browser");

    // Enter submits the form as a POST to the resolved action with the query.
    Hummingbird::InputEvent enter_event;
    enter_event.type = Hummingbird::EventType::KeyDown;
    enter_event.key.key = Hummingbird::Key::Enter;
    auto result = harness.tab().handle_key_down(enter_event);
    EXPECT_TRUE(result.handled);
    ASSERT_TRUE(result.submitted_form.has_value());
    EXPECT_EQ(result.submitted_form->url, "https://html.duckduckgo.com/html/");
    EXPECT_EQ(result.submitted_form->method, Hummingbird::Engine::FormSubmitMethod::Post);
    EXPECT_NE(result.submitted_form->body.find("q=hummingbird+browser"), std::string::npos)
        << "form body was: " << result.submitted_form->body;
}

TEST(EngineTabTest, ClickSubmitReturnsPostFormSubmission) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form action="/html/" method="post">
      <input type="submit" value="Search">
      <input name="q" value="venus">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    auto submitted = harness.tab().submit_form_at(point, harness.viewport());
    ASSERT_TRUE(submitted.has_value());
    EXPECT_EQ(submitted->url, "https://example.dev/html/");
    EXPECT_EQ(submitted->method, Hummingbird::Engine::FormSubmitMethod::Post);
    EXPECT_EQ(submitted->body, "q=venus");
    EXPECT_EQ(submitted->content_type, "application/x-www-form-urlencoded");
}

TEST(EngineTabTest, SubmitsButtonWithFormAttribute) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <button form="search-form">Search</button>
    <form id="search-form" action="/search" method="get">
      <input name="q" value="moon">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    auto submitted = harness.tab().submit_form_at(point, harness.viewport());
    ASSERT_TRUE(submitted.has_value());
    EXPECT_EQ(submitted->url, "https://example.dev/search?q=moon");
    EXPECT_EQ(submitted->method, Hummingbird::Engine::FormSubmitMethod::Get);
}

TEST(EngineTabTest, ButtonWithEmptyTypeDefaultsToSubmit) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <form action="/search" method="get">
      <button type="">Search</button>
      <input name="q" value="mars">
    </form>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               std::move(provider), nullptr);
    harness.set_viewport({0, 0, 300, 200});
    harness.navigate("https://example.dev");
    ASSERT_TRUE(harness.tick());

    Hummingbird::Layout::Point point{12.0f, 12.0f};
    auto submitted = harness.tab().submit_form_at(point, harness.viewport());
    ASSERT_TRUE(submitted.has_value());
    EXPECT_EQ(submitted->url, "https://example.dev/search?q=mars");
    EXPECT_EQ(submitted->method, Hummingbird::Engine::FormSubmitMethod::Get);
}

TEST(EngineTabTest, UpdatesRequestedUrlFromEffectiveUrl) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p>Redirected</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    const std::string effective_url = "https://www.acme.com/software/thttpd/";
    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html, effective_url),
                               std::make_unique<InlineNetwork>(html, effective_url), std::move(provider), nullptr);
    harness.set_viewport({0, 0, 200, 200});

    harness.navigate("http://acme.com/software/thttpd");
    EXPECT_TRUE(harness.tick());

    EXPECT_EQ(harness.tab().requested_url(), effective_url);
}

TEST(EngineTabTest, ResolvesRelativeResourcesFromNormalizedUrl) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="styles/site.css">
  </head>
  <body>
    <img src="../img/logo.png" alt="logo">
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test/dir/page.html", html);
    network->set_response("https://acme.test/dir/styles/site.css", "body { color: red; }");
    network->set_response("https://acme.test/img/logo.png", "PNGDATA");

    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider),
                               std::make_unique<InlineImageDecoder>());

    harness.navigate("acme.test/dir/page.html");
    EXPECT_TRUE(harness.tick());
    EXPECT_TRUE(harness.tick());

    const auto& requested = network_ptr->requested();
    EXPECT_NE(std::find(requested.begin(), requested.end(), "https://acme.test/dir/page.html"), requested.end());
    EXPECT_NE(std::find(requested.begin(), requested.end(), "https://acme.test/dir/styles/site.css"), requested.end());
    EXPECT_NE(std::find(requested.begin(), requested.end(), "https://acme.test/img/logo.png"), requested.end());
    EXPECT_EQ(harness.tab().requested_url(), "https://acme.test/dir/page.html");
}

TEST(EngineTabTest, MarksResourcesFailedWhenFetchReturnsEmpty) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <img src="missing.png" alt="missing">
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://acme.test/page.html", html);
    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);

    harness.navigate("https://acme.test/page.html");
    EXPECT_TRUE(harness.tick());

    auto view = harness.resource_view("https://acme.test/missing.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Loading);

    EXPECT_FALSE(harness.tick());
    view = harness.resource_view("https://acme.test/missing.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Failed);
}

TEST(EngineTabTest, ClearsResourcesOnNavigationSwap) {
    const std::string html_first = R"HTML(
<!doctype html>
<html>
  <body>
    <img src="first.png" alt="first">
  </body>
</html>
)HTML";

    const std::string html_second = R"HTML(
<!doctype html>
<html>
  <body>
    <img src="second.png" alt="second">
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://acme.test/first.html", html_first);
    network->set_response("https://acme.test/second.html", html_second);
    network->set_response("https://acme.test/first.png", "FIRSTPNG");
    network->set_response("https://acme.test/second.png", "SECONDPING");

    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider),
                               std::make_unique<InlineImageDecoder>());

    harness.navigate("https://acme.test/first.html");
    EXPECT_TRUE(harness.tick());
    EXPECT_TRUE(harness.tick());

    auto first_view = harness.resource_view("https://acme.test/first.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(first_view.has_value());
    EXPECT_EQ(first_view->state, Hummingbird::Engine::ResourceState::Ready);

    harness.navigate("https://acme.test/second.html");
    EXPECT_TRUE(harness.tick());

    first_view = harness.resource_view("https://acme.test/first.png", Hummingbird::Engine::ResourceType::Image);
    EXPECT_FALSE(first_view.has_value());

    EXPECT_TRUE(harness.tick());
    auto second_view = harness.resource_view("https://acme.test/second.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(second_view.has_value());
    EXPECT_EQ(second_view->state, Hummingbird::Engine::ResourceState::Ready);
}

TEST(EngineTabTest, RequestsStylesheetsInDocumentOrder) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="a.css">
    <link rel="stylesheet" href="b.css">
  </head>
  <body>
    <p>Order</p>
  </body>
</html>
)HTML";

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test/page.html", html);
    network->set_response("https://acme.test/a.css", "p { color: red; }");
    network->set_response("https://acme.test/b.css", "p { color: blue; }");

    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);

    harness.navigate("https://acme.test/page.html");
    EXPECT_TRUE(harness.tick());

    const auto& requested = network_ptr->requested();
    auto it_a = std::find(requested.begin(), requested.end(), "https://acme.test/a.css");
    auto it_b = std::find(requested.begin(), requested.end(), "https://acme.test/b.css");
    ASSERT_NE(it_a, requested.end());
    ASSERT_NE(it_b, requested.end());
    EXPECT_LT(std::distance(requested.begin(), it_a), std::distance(requested.begin(), it_b));
}

TEST(EngineTabTest, ShowsTlsWarningPageOnVerificationFailure) {
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<TlsFailureNetwork>("<html><body>ok</body></html>");
    auto* network_ptr = network.get();
    auto fallback = std::make_unique<InlineNetwork>("<html><body>fallback</body></html>");

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);
    harness.navigate("https://badcert.test");
    EXPECT_TRUE(harness.tick());

    auto view = harness.resource_view("https://badcert.test", Hummingbird::Engine::ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_NE(view->body.find("Secure connection failed"), std::string::npos);
    EXPECT_NE(view->body.find("https://badcert.test"), std::string::npos);
    EXPECT_EQ(harness.tab().security_state(), Hummingbird::SecurityState::InsecureTls);
    EXPECT_FALSE(network_ptr->last_allow_insecure());
}

TEST(EngineTabTest, AllowsInsecureReloadForCurrentHost) {
    const std::string html = "<html><body>ok</body></html>";
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<TlsFailureNetwork>(html);
    auto* network_ptr = network.get();
    auto fallback = std::make_unique<InlineNetwork>("<html><body>fallback</body></html>");

    HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider), nullptr);
    harness.navigate("https://badcert.test");
    EXPECT_TRUE(harness.tick());

    EXPECT_TRUE(harness.tab().allow_insecure_for_current_host());
    harness.tab().navigate(harness.tab().requested_url());
    EXPECT_TRUE(harness.tick());

    auto view = harness.resource_view("https://badcert.test", Hummingbird::Engine::ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_NE(view->body.find("ok"), std::string::npos);
    EXPECT_TRUE(network_ptr->last_allow_insecure());
    EXPECT_EQ(harness.tab().security_state(), Hummingbird::SecurityState::InsecureTls);
}

TEST(EngineTabTest, DefersScriptExecutionAndLoadUntilExternalScriptArrives) {
    // 7.0.1: with an external <script src> still in flight, no script (and no
    // load event) runs; once the fetch completes, scripts run in document
    // order and their DOM writes reach layout. Observable via content height:
    // the script fills #out with enough text to wrap into multiple lines.
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p id="out">x</p>
    <script src="app.js"></script>
  </body>
</html>
)HTML";

    std::string filler;
    for (int i = 0; i < 60; ++i) filler += "wrapped words grow the layout ";
    const std::string script = "document.getElementById('out').textContent = '" + filler + "';";

    auto network = std::make_unique<DeferredNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test", html);
    network->defer_response("https://acme.test/app.js", script);

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    HeadlessTabHarness harness(std::move(network), std::make_unique<DeferredNetwork>(), std::move(provider), nullptr);

    harness.navigate("https://acme.test");
    harness.tick();  // document ready: parse, request script, defer execution
    harness.tick();  // nothing new: script still in flight

    auto pending = harness.resource_view("https://acme.test/app.js", Hummingbird::Engine::ResourceType::Script);
    ASSERT_TRUE(pending.has_value());
    EXPECT_NE(pending->state, Hummingbird::Engine::ResourceState::Ready);
    const float height_before = harness.tab().content_height();

    network_ptr->complete("https://acme.test/app.js");
    harness.tick();  // script batch: run deferred scripts, rebuild, dispatch load

    auto ready = harness.resource_view("https://acme.test/app.js", Hummingbird::Engine::ResourceType::Script);
    ASSERT_TRUE(ready.has_value());
    EXPECT_EQ(ready->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_GT(harness.tab().content_height(), height_before);
}

TEST(EngineTabTest, ManyMutationsInOneHandlerProduceExactlyOnePass) {
    // 7.4.1 invalidation budget: a click handler that makes 100 DOM mutations
    // must produce exactly one style+layout pass, not one per mutation.
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p id="hit">click me</p>
    <ul id="list"></ul>
    <script>
      // Delegate on document so any bubbling click triggers it (7.4.1 test).
      document.addEventListener('click', function () {
        var list = document.getElementById('list');
        for (var i = 0; i < 100; i++) {
          var li = document.createElement('li');
          li.textContent = 'n' + i;
          list.appendChild(li);
        }
      });
    </script>
  </body>
</html>
)HTML";

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://acme.test", html);
    auto fallback = std::make_unique<RoutingNetwork>();

    HeadlessTabHarness harness(std::move(network), std::move(fallback), Hummingbird::create_resource_provider(),
                               nullptr);
    harness.navigate("https://acme.test");
    ASSERT_TRUE(harness.tick());  // build + run scripts (registers the handler)

    // Baseline after load; the click's 100 mutations should add exactly one pass.
    const size_t passes_before = harness.tab().style_layout_pass_count();
    auto result = harness.dispatch_click({12.0f, 12.0f});  // over the top-left "click me" text
    EXPECT_TRUE(result.mutated);                           // the handler ran and changed the DOM
    EXPECT_EQ(harness.tab().style_layout_pass_count(), passes_before + 1);
}

TEST(EngineTabTest, M7DemoPageLoadsExternalScriptFromAssets) {
    // Guards the shipped example.dev/m7 demo: its <script src> must resolve
    // through the bundled-asset probe and register Ready so the page's
    // document-order sequence actually runs in the app.
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto html = provider->load_text("assets/stub/pages/m7.html");
    ASSERT_TRUE(html.has_value()) << "demo page asset missing";

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(*html), std::make_unique<InlineNetwork>(*html),
                               Hummingbird::create_resource_provider(), nullptr);

    harness.navigate("https://example.dev/m7");
    harness.tick();
    harness.tick();

    auto script =
        harness.resource_view("https://example.dev/assets/stub/pages/m7.js", Hummingbird::Engine::ResourceType::Script);
    ASSERT_TRUE(script.has_value()) << "external demo script was never requested";
    EXPECT_EQ(script->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_NE(script->body.find("step-external"), std::string::npos);
}
