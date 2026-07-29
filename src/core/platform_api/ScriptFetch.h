#pragma once

#include <cstdint>
#include <string>

#include "core/net/HttpHeaders.h"

namespace Hummingbird {

// The `fetch()` seam between the script adapter and the engine (story 9.1.1).
//
// fetch is the first binding that cannot answer synchronously: it must hand JS a
// Promise now and settle it later, from a network callback that runs on a
// transport thread. The script engine is single-threaded and its context is not
// thread-safe, so the engine queues the result and settles it on the main thread
// during the ordinary tick, exactly as document and subresource loads already
// work. These types are what crosses that queue.
//
// Bodies are buffered, not streamed (an explicit M9 non-goal).

// A JS-initiated request, already normalized by the adapter.
struct ScriptFetchRequest {
    // Absolute URL. The adapter resolves relative URLs against the document
    // before this point, because only it knows the document's base.
    std::string url;
    std::string method = "GET";
    Core::HttpHeaders headers;
    // Empty for GET/HEAD. Present verbatim for anything else; the caller has
    // already chosen the Content-Type header if it wanted one.
    std::string body;
    bool has_body = false;
};

// Why a fetch could not produce a response at all. A *server* answering 404 is
// NOT a failure here: per the Fetch standard a fetch promise rejects only on a
// network error, so a 404 resolves with ok == false. Only these reject it.
enum class ScriptFetchFailure {
    None,
    // Malformed or unsupported URL, or a scheme the engine will not fetch.
    BadUrl,
    // Ran out of its time budget (story 9.1.3).
    Timeout,
    // Everything else the transport could not complete: DNS, refused, TLS.
    NetworkError,
};

struct ScriptFetchResponse {
    // Correlates with the id returned when the request was started, so the
    // adapter can find the promise this settles.
    std::uint64_t id = 0;
    ScriptFetchFailure failure = ScriptFetchFailure::None;
    long status = 0;
    // Where the response actually came from, after any redirects.
    std::string url;
    Core::HttpHeaders headers;
    std::string body;

    bool ok() const { return failure == ScriptFetchFailure::None && status >= 200 && status <= 299; }
};

}  // namespace Hummingbird
