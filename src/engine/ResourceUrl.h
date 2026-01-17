#pragma once

#include <string>
#include <string_view>

#include "core/utils/Url.h"

namespace Hummingbird::Engine {

struct ResolvedUrl {
    std::string resolved;
    std::string key;
};

inline ResolvedUrl resolve_resource_url(std::string_view base_url, std::string_view raw_url) {
    ResolvedUrl result;
    result.resolved = Core::resolve_url(base_url, raw_url);
    if (result.resolved.empty()) {
        result.key = std::string(raw_url);
    } else {
        result.key = result.resolved;
    }
    return result;
}

}  // namespace Hummingbird::Engine
