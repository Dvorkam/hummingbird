// T-COOKIE-CONFORMANCE-VECTORS-1: the cookie module's adherence as a NUMBER.
//
// `doc/conformance/rfc6265_cookies.md` describes what we adhere to. Prose alone
// rots into confident lies, which is why that register's own anti-rot rule asks
// for something executable beside it. This is that: a pinned table whose pass
// count may only rise.
//
// Deliberately NOT web-platform-tests. WPT's cookie suite is browser-driven —
// testharness.js, a wptserve instance with dedicated host aliases, assertions
// made through fetch. The cookie core is a pure function of (Set-Cookie, URL,
// clock), so a vector table tests the same semantics for a fraction of the
// machinery. Use the cheap ratchet where it exists; record the number either
// way.
#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/net/CookieJar.h"

namespace {
using Hummingbird::Core::CookieJar;
using Hummingbird::Core::CookieRequestContext;
using Hummingbird::Core::HttpHeaders;

struct Step {
    bool is_get = false;
    std::string url;
    std::string payload;  // Set-Cookie value, or the expected Cookie header
};

struct Vector {
    std::string name;
    std::vector<Step> steps;
    std::string xfail_ticket;
    std::string xfail_reason;
    int line = 0;
};

std::string trim(std::string_view text) {
    size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return {};
    size_t end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(begin, end - begin + 1));
}

std::vector<Vector> load_vectors(std::string* error) {
    std::vector<Vector> vectors;
    std::ifstream file(std::string(HB_TEST_FIXTURE_DIR) + "/cookies/rfc6265_vectors.txt", std::ios::binary);
    if (!file) {
        *error = "cookies/rfc6265_vectors.txt could not be opened";
        return vectors;
    }

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        const std::string text = trim(line);
        if (text.empty() || text[0] == '#') continue;

        if (text.rfind("case ", 0) == 0) {
            Vector vector;
            vector.name = trim(text.substr(5));
            vector.line = line_number;
            vectors.push_back(std::move(vector));
            continue;
        }
        if (vectors.empty()) {
            *error = "directive before the first `case` at line " + std::to_string(line_number);
            return vectors;
        }
        Vector& current = vectors.back();

        if (text.rfind("xfail ", 0) == 0) {
            std::istringstream stream(text.substr(6));
            stream >> current.xfail_ticket;
            std::getline(stream, current.xfail_reason);
            current.xfail_reason = trim(current.xfail_reason);
            continue;
        }

        const bool is_get = text.rfind("get ", 0) == 0;
        const bool is_set = text.rfind("set ", 0) == 0;
        if (!is_get && !is_set) {
            *error = "unrecognised directive at line " + std::to_string(line_number) + ": " + text;
            return vectors;
        }
        const size_t bar = text.find('|');
        if (bar == std::string::npos) {
            *error = "missing '|' at line " + std::to_string(line_number);
            return vectors;
        }
        Step step;
        step.is_get = is_get;
        step.url = trim(text.substr(4, bar - 4));
        step.payload = trim(text.substr(bar + 1));
        current.steps.push_back(std::move(step));
    }
    return vectors;
}

// Runs one vector against a fresh jar. Returns empty on success, or a
// description of the first mismatch.
std::string run_vector(const Vector& vector) {
    CookieJar jar;
    // A fixed instant, so `Max-Age=3600` and an expiry in 2020 mean the same
    // thing on every run. A vector table that drifts with the wall clock is a
    // flake generator, not a ratchet.
    const auto now = std::chrono::system_clock::from_time_t(1767225600);  // 2026-01-01T00:00:00Z

    for (const auto& step : vector.steps) {
        if (!step.is_get) {
            HttpHeaders headers;
            if (!step.payload.empty()) headers.add("Set-Cookie", step.payload);
            jar.store_from_response(step.url, headers, now);
            continue;
        }
        CookieRequestContext context;  // a same-site top-level GET
        const std::string actual = jar.cookie_header_for(step.url, now, context);
        if (actual != step.payload) {
            return "GET " + step.url + "\n      expected: [" + step.payload + "]\n      actual:   [" + actual + "]";
        }
    }
    return {};
}
}  // namespace

// One test, reporting a count. Individually-named tests would give a nicer
// failure list but would make the NUMBER — the thing this story is for —
// something a reader has to assemble by eye from the ctest output.
TEST(CookieConformanceTest, PinnedVectorsReportAnAdherenceCount) {
    std::string error;
    const auto vectors = load_vectors(&error);
    ASSERT_TRUE(error.empty()) << error;
    // A silently empty or truncated fixture would turn this into a test that
    // always passes, which is the failure mode a conformance number must not
    // have.
    ASSERT_GT(vectors.size(), 30u) << "rfc6265_vectors.txt missing or truncated";

    size_t passed = 0;
    size_t expected_failures = 0;
    std::vector<std::string> regressions;
    std::vector<std::string> unexpected_passes;

    for (const auto& vector : vectors) {
        const std::string failure = run_vector(vector);
        const bool is_xfail = !vector.xfail_ticket.empty();
        if (failure.empty()) {
            ++passed;
            if (is_xfail) {
                unexpected_passes.push_back(vector.name + " (marked xfail " + vector.xfail_ticket + ")");
            }
            continue;
        }
        if (is_xfail) {
            ++expected_failures;
            continue;
        }
        regressions.push_back("  [line " + std::to_string(vector.line) + "] " + vector.name + "\n      " + failure);
    }

    // The number, printed on every run so it is visible in CI output rather
    // than only on failure.
    std::cout << "[cookie-conformance] " << passed << "/" << vectors.size() << " vectors passing";
    if (expected_failures > 0) {
        std::cout << " (" << expected_failures << " known-failing, each naming a ticket)";
    }
    std::cout << std::endl;

    std::string report;
    for (const auto& line : regressions) report += line + "\n";
    EXPECT_TRUE(regressions.empty()) << regressions.size() << " vector(s) regressed:\n" << report;

    // A vector that starts passing must lose its xfail, or the count
    // understates adherence and the ticket it names looks unfixed forever.
    std::string passes;
    for (const auto& line : unexpected_passes) passes += "  " + line + "\n";
    EXPECT_TRUE(unexpected_passes.empty())
        << "vector(s) now pass and must have their xfail removed:\n"
        << passes;
}
