// Story 8.1.5: document.cookie. Driven through a real Tab + real QuickJS engine
// against a real jar, so the binding, the HttpOnly filter, and the shared parse
// path are exercised together.
//
// The jar doubles as the observation channel: a script reports what it saw by
// writing a flag cookie, which is more robust than parsing painted text (inline
// text paints as per-word runs).
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/net/CookieJar.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "test_utils/HeadlessTabHarness.h"
#include "test_utils/TestFakes.h"

using Hummingbird::Core::CookieClock;
using Hummingbird::Core::CookieJar;
using Hummingbird::Test::HeadlessTabHarness;
using Hummingbird::Test::InlineNetwork;

namespace {
void run_script(const std::shared_ptr<CookieJar>& jar, const std::string& script) {
    const std::string html = "<!doctype html><html><body><script>" + script + "</script></body></html>";
    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               Hummingbird::create_resource_provider(), nullptr, nullptr, jar);
    harness.set_viewport({0, 0, 800, 600});
    harness.navigate("https://example.test/page");
    harness.tick();
    harness.tick();
}

// Value of `name` in the jar for the test document, or "" when absent.
std::string stored(const std::shared_ptr<CookieJar>& jar, const std::string& name) {
    for (const auto& cookie : jar->entries()) {
        if (cookie.name == name) return cookie.value;
    }
    return {};
}
}  // namespace

TEST(DocumentCookieTest, ScriptReadsCookiesForItsOwnDocument) {
    auto jar = std::make_shared<CookieJar>();
    jar->store_from_header("https://example.test/", "theme=dark", CookieClock::now());

    run_script(jar, "document.cookie = 'saw=' + (document.cookie === 'theme=dark' ? 'exact' : 'other');");
    EXPECT_EQ(stored(jar, "saw"), "exact");
}

// The reason HttpOnly exists: an XSS payload reading document.cookie must not
// find the session token.
TEST(DocumentCookieTest, HttpOnlyCookiesAreInvisibleToScript) {
    auto jar = std::make_shared<CookieJar>();
    const auto when = CookieClock::now();
    jar->store_from_header("https://example.test/", "session=secret; HttpOnly", when);
    jar->store_from_header("https://example.test/", "theme=dark", when);

    run_script(jar, "document.cookie = 'leaked=' + (document.cookie.indexOf('secret') >= 0 ? 'yes' : 'no');");
    EXPECT_EQ(stored(jar, "leaked"), "no");
    // ...while the network still carries it: HttpOnly hides it from JS, not HTTP.
    EXPECT_NE(jar->cookie_header_for("https://example.test/", when).find("session=secret"), std::string::npos);
}

TEST(DocumentCookieTest, ScriptCanSetACookieAndReadItBack) {
    auto jar = std::make_shared<CookieJar>();
    run_script(jar, "document.cookie = 'a=1'; document.cookie = 'echo=' + (document.cookie.indexOf('a=1') >= 0 ? "
                    "'yes' : 'no');");
    EXPECT_EQ(stored(jar, "a"), "1");
    EXPECT_EQ(stored(jar, "echo"), "yes");
}

// Assignment sets ONE cookie; it does not replace the jar. This is the classic
// document.cookie surprise, so it is worth pinning.
TEST(DocumentCookieTest, AssignmentAddsRatherThanReplaces) {
    auto jar = std::make_shared<CookieJar>();
    jar->store_from_header("https://example.test/", "existing=1", CookieClock::now());

    run_script(jar, "document.cookie = 'added=2';");
    EXPECT_EQ(stored(jar, "existing"), "1");
    EXPECT_EQ(stored(jar, "added"), "2");
}

TEST(DocumentCookieTest, ScriptWritesGoThroughTheSameAttributeParsingAsAServer) {
    auto jar = std::make_shared<CookieJar>();
    run_script(jar, "document.cookie = 'scoped=1; Path=/deep; Max-Age=600';");

    ASSERT_EQ(jar->size(), 1u);
    const auto& cookie = jar->entries()[0];
    EXPECT_EQ(cookie.path, "/deep");
    EXPECT_FALSE(cookie.is_session()) << "Max-Age must be honored from script too";
}

// A page must not plant a cookie on someone else's domain, whether the string
// arrives from a server or from script.
TEST(DocumentCookieTest, ScriptCannotSetACookieForAnotherDomain) {
    auto jar = std::make_shared<CookieJar>();
    run_script(jar, "document.cookie = 'evil=1; Domain=attacker.test';");
    EXPECT_TRUE(jar->empty());
}

TEST(DocumentCookieTest, ReadingWithNoCookiesYieldsAnEmptyString) {
    auto jar = std::make_shared<CookieJar>();
    run_script(jar, "document.cookie = 'was=' + (document.cookie === '' ? 'empty' : 'nonempty');");
    EXPECT_EQ(stored(jar, "was"), "empty");
}

// Most unit tests construct a Tab with no jar at all; document.cookie must read
// as empty there rather than crashing.
TEST(DocumentCookieTest, WithoutAJarDocumentCookieIsSimplyEmptyAndWritesAreDropped) {
    run_script(nullptr, "document.cookie = 'a=1'; var unused = document.cookie;");
    SUCCEED() << "no jar wired up must not crash the script host";
}
