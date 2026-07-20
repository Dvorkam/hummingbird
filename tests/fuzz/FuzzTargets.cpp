#include "fuzz/FuzzTargets.h"

#include <string_view>

#include "core/ArenaAllocator.h"
#include "html/HtmlParser.h"
#include "style/parser/CssParser.h"

namespace Hummingbird::Fuzz {

void fuzz_html(const uint8_t* data, size_t size) {
    std::string_view input(reinterpret_cast<const char*>(data), size);
    // A bounded arena: pathological input that would blow past the budget makes
    // the parser fail gracefully rather than allocating without limit.
    Core::ArenaAllocator arena(4 * 1024 * 1024);
    Html::Parser parser(arena, input);
    auto result = parser.parse();
    (void)result;
}

void fuzz_css(const uint8_t* data, size_t size) {
    std::string_view input(reinterpret_cast<const char*>(data), size);
    Css::Parser parser(input);
    auto sheet = parser.parse();
    (void)sheet;
}

}  // namespace Hummingbird::Fuzz
