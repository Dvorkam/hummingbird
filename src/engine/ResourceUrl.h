#pragma once

#include <string>
#include <string_view>

#include "core/utils/Url.h"

namespace Hummingbird::Engine {

struct ResolvedUrl {
    std::string resolved;
    std::string_view key;
};

inline ResolvedUrl resolve_resource_url(std::string_view base_url, std::string_view raw_url) {
    ResolvedUrl result;
    result.resolved = Core::resolve_url(base_url, raw_url);
    result.key = result.resolved.empty() ? raw_url : std::string_view(result.resolved);
    return result;
}

}  // namespace Hummingbird::Engine
