#pragma once

#include <functional>
#include <optional>
#include <string_view>

namespace Hummingbird::Engine {

// Resolves an external <script src> to its fetched body (nullopt when the
// fetch failed or never resolved — the script is skipped with a warning).
// Returned views must stay valid for the duration of the script run; they
// point into the resource store.
using ExternalScriptLookup = std::function<std::optional<std::string_view>(std::string_view src)>;

}  // namespace Hummingbird::Engine
