#include <gtest/gtest.h>

#include <algorithm>
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
    ASSERT_TRUE(result.submitted_url.has_value());
    EXPECT_EQ(*result.submitted_url, "https://example.dev/search?q=hello%20world&lang=en");
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
    ASSERT_TRUE(result.submitted_url.has_value());
    EXPECT_EQ(*result.submitted_url, "https://example.dev/search?q=test");
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
    EXPECT_EQ(*submitted, "https://example.dev/search?q=moon");
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
    EXPECT_EQ(*submitted, "https://example.dev/search?q=mars");
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
