#pragma once

#include <cstdint>
#include <string_view>

namespace Hummingbird {

class IExtensionApiHost {
public:
    virtual ~IExtensionApiHost() = default;

    // Returns true when CSS was accepted for injection into the target tab.
    virtual bool insert_css(std::uint32_t tab_id, std::string_view css_text) = 0;
};

}  // namespace Hummingbird
