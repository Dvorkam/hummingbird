#pragma once

#include <functional>
#include <optional>
#include <string_view>

namespace Hummingbird::Engine {

// The answer to "can this external <script src> run, and if not, why not".
//
// The `blocked_by_filter` flag exists because "no body" has two meanings that
// must not be reported the same way (story 9.4.1): a script that failed to load
// is a problem worth a warning, while a script a filter rule deliberately
// refused is the feature working. Without the distinction, turning on an ad
// blocker fills the log with warnings about the requests it was installed to
// prevent — which is how real warnings stop being read.
struct ExternalScriptSource {
    // The script's bytes, present only when it is available to run. Views must
    // stay valid for the duration of the script run; they point into the
    // resource store.
    std::optional<std::string_view> body;
    bool blocked_by_filter = false;
};

// Resolves an external <script src> to its fetched body.
using ExternalScriptLookup = std::function<ExternalScriptSource(std::string_view src)>;

}  // namespace Hummingbird::Engine
