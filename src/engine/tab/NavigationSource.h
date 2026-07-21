#pragma once

namespace Hummingbird::Engine {

// What caused a navigation.
//
// This is a cookie-policy input, not bookkeeping: SameSite has to know whether
// the loaded document initiated the request, and only the caller knows that
// (story 8.1.2 / T-COOKIE-NAV-INITIATOR-1). It lives in its own header so the
// app layer can name it without including all of Tab.
enum class NavigationSource {
    // Address bar, bookmark, history: there is no initiating document, so
    // nothing is cross-site and every cookie is eligible.
    User,
    // A link click or form submit in the loaded document, which becomes the
    // initiator for the same-site comparison.
    Document,
};

}  // namespace Hummingbird::Engine
