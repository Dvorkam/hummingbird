#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Css {

std::string merge_css_sources(std::string_view ua_css, const std::vector<std::string>& link_sources,
                              const std::vector<std::string>& style_blocks,
                              const std::vector<std::string>& extension_sources = {});

}  // namespace Hummingbird::Css
