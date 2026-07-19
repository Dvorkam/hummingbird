#pragma once

#include <cstddef>
#include <cstdint>

// Shared "one input" bodies for the parser fuzzers (story 7.5.3). The libFuzzer
// entry points (html_parser_fuzzer.cpp / css_parser_fuzzer.cpp) and the
// cross-platform smoke test (ParserFuzzSmoke.test.cpp) all funnel through these,
// so the exact same harness is exercised by the normal test suite and by
// libFuzzer runs in CI.
namespace Hummingbird::Fuzz {

// Feed arbitrary bytes to the HTML parser; must never crash, hang, or corrupt
// memory regardless of input.
void fuzz_html(const uint8_t* data, size_t size);

// Feed arbitrary bytes to the CSS parser; same contract.
void fuzz_css(const uint8_t* data, size_t size);

}  // namespace Hummingbird::Fuzz
