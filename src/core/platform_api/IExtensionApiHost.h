#pragma once

#include <cstdint>
#include <string_view>

namespace Hummingbird {

// What an extension's background script can ask the browser to do.
//
// Every method takes `extension_id` as its first parameter, and that is the
// whole reason this signature changed in story 9.4.1. The `permissions` manifest
// field had been parsed and never enforced — not from oversight, but because a
// host that cannot tell WHO is calling has nowhere to put the check. One host
// instance serves every extension; identity has to travel with the call.
//
// A manifest field that is parsed and not enforced is worse than an absent one:
// it reads as a boundary and is not one.
class IExtensionApiHost {
public:
    virtual ~IExtensionApiHost() = default;

    // Returns true when CSS was accepted for injection into the target tab.
    // False covers both "no handler" and "this extension lacks the permission",
    // which the caller cannot distinguish — deliberately, since an extension
    // learning exactly why it was refused is of no use to it.
    virtual bool insert_css(std::string_view extension_id, std::uint32_t tab_id, std::string_view css_text) = 0;
};

}  // namespace Hummingbird
