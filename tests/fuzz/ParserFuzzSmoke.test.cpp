#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "fuzz/FuzzTargets.h"

namespace {
namespace fs = std::filesystem;

std::vector<uint8_t> read_bytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::vector<fs::path> corpus_files(const std::string& subdir) {
    std::vector<fs::path> out;
    fs::path dir = fs::path(HB_FUZZ_CORPUS_DIR) / subdir;
    if (!fs::is_directory(dir)) return out;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) out.push_back(entry.path());
    }
    return out;
}

void feed(void (*fuzz)(const uint8_t*, size_t), const char* s) {
    fuzz(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
}
}  // namespace

// The seed corpus and a set of adversarial snippets must parse without crashing,
// hanging, or corrupting memory. This is the cross-platform guard (story 7.5.3);
// libFuzzer explores far beyond it in the dedicated CI job. Crashing inputs found
// by libFuzzer are checked back into the corpus so this test locks the fix in.
TEST(ParserFuzzSmokeTest, HtmlCorpusAndAdversarialInputsDoNotCrash) {
    const auto files = corpus_files("html");
    ASSERT_FALSE(files.empty()) << "html seed corpus missing (tests/fuzz/corpus/html)";
    for (const auto& f : files) {
        auto bytes = read_bytes(f);
        Hummingbird::Fuzz::fuzz_html(bytes.data(), bytes.size());
    }
    // Truncations, unterminated constructs, and nesting that stress the parser.
    for (const char* s : {"", "<", "<<<<", "<a", "<a href=\"", "<!--", "<!doctype", "<div><span><p>", "</></></>",
                          "<svg><rect><circle>", "&#x", "&amp", "<a b=c d=", "<table><tr><td>", "<script>x",
                          "<style>a{", "<p>&#xZZ;</p>", "<!-- <div> -->"}) {
        feed(&Hummingbird::Fuzz::fuzz_html, s);
    }
    SUCCEED();
}

TEST(ParserFuzzSmokeTest, CssCorpusAndAdversarialInputsDoNotCrash) {
    const auto files = corpus_files("css");
    ASSERT_FALSE(files.empty()) << "css seed corpus missing (tests/fuzz/corpus/css)";
    for (const auto& f : files) {
        auto bytes = read_bytes(f);
        Hummingbird::Fuzz::fuzz_css(bytes.data(), bytes.size());
    }
    for (const char* s :
         {"", "{", "}", "a{", "a{b", "a{b:", "a{b:c", "@media", "@media(", "/*", "/* unterminated", ":::", "a,,,b{}",
          "a{color:rgb(", "url(", "@import ", ".x{background:url(", "a { } b { } c {", "@keyframes x{"}) {
        feed(&Hummingbird::Fuzz::fuzz_css, s);
    }
    SUCCEED();
}
