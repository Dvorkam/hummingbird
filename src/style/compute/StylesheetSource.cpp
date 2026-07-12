#include "style/compute/StylesheetSource.h"

namespace Hummingbird::Css {
namespace {
void append_block(std::string& out, std::string_view block) {
    if (block.empty()) {
        return;
    }
    if (!out.empty() && out.back() != '\n') {
        out.push_back('\n');
    }
    out.append(block);
    if (!out.empty() && out.back() != '\n') {
        out.push_back('\n');
    }
}
}  // namespace

std::string merge_css_sources(std::string_view ua_css, const std::vector<std::string>& link_sources,
                              const std::vector<std::string>& style_blocks,
                              const std::vector<std::string>& extension_sources) {
    std::string merged;
    append_block(merged, ua_css);
    for (const auto& link_css : link_sources) {
        append_block(merged, link_css);
    }
    for (const auto& block : style_blocks) {
        append_block(merged, block);
    }
    for (const auto& extension_css : extension_sources) {
        append_block(merged, extension_css);
    }
    return merged;
}

}  // namespace Hummingbird::Css
