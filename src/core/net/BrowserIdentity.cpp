#include "core/net/BrowserIdentity.h"

#include "core/Version.h"

namespace Hummingbird::Core {

namespace {

// Platform tokens are hard-coded to Windows for now. Making them track the build
// target (or a spoofed value in Compatibility mode) is T-NET-IDENTITY-PLATFORM-1.
constexpr const char* kOsToken = "Windows NT 10.0; Win64; x64";
constexpr const char* kPlatform = "\"Windows\"";

// The truthful client-hint brand list, identical in both modes. "Not_A Brand" is
// the GREASE placeholder real browsers include; we deliberately do NOT list
// "Chromium", because Hummingbird is not Chromium.
constexpr const char* kSecChUa = "\"Hummingbird\";v=\"0\", \"Not_A Brand\";v=\"99\"";

std::string compatibility_user_agent() {
    return std::string("Mozilla/5.0 (") + kOsToken +
           ") AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
}

}  // namespace

std::string transparent_user_agent() {
    // Tracks the real project version (HB_VERSION_STRING) so it never drifts.
    return std::string("Hummingbird/") + HB_VERSION_STRING + " (" + kOsToken + ")";
}

std::vector<IdentityHeader> identity_headers(IdentityMode mode, bool secure) {
    std::vector<IdentityHeader> headers;
    headers.push_back(
        {"User-Agent", mode == IdentityMode::Compatibility ? compatibility_user_agent() : transparent_user_agent()});

    // Client hints ride only on secure requests, exactly as a real browser sends
    // them. The brand list is truthful regardless of the UA-string mode.
    if (secure) {
        headers.push_back({"Sec-CH-UA", kSecChUa});
        headers.push_back({"Sec-CH-UA-Mobile", "?0"});
        headers.push_back({"Sec-CH-UA-Platform", kPlatform});
    }
    return headers;
}

}  // namespace Hummingbird::Core
