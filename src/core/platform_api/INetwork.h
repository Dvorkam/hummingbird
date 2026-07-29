#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "core/net/HttpHeaders.h"

namespace Hummingbird {

enum class NetworkError {
    None,
    TlsVerificationFailed,
    CurlError,
    // The engine follows redirects itself (story 8.3.1) so it can apply cookie
    // and method policy per hop; these are its two termination conditions.
    TooManyRedirects,
    RedirectLoop,
    // The request ran out of its time budget (story 9.1.3). Distinct from
    // CurlError so the error page can say "timed out" rather than "could not be
    // reached", and so a fetch promise can reject with a timeout-shaped reason
    // instead of a generic network failure.
    Timeout,
    // The ENGINE refused this request or its response, rather than the transport
    // failing (story 9.2.3): a CORS check that a redirect hop did not pass. It is
    // a NetworkError variant because the redirect loop must be able to abandon a
    // chain mid-flight, and that loop only speaks this vocabulary.
    CorsBlocked,
};

struct NetworkRequestOptions {
    bool allow_insecure = false;
    std::string content_type;
    // Extra request headers (Cookie, ...). The backend owns transport headers
    // such as Content-Type and Accept-Encoding; these are added alongside.
    Core::HttpHeaders headers;

    // Deadlines for THIS call, in milliseconds; 0 means "use the backend's own
    // default" (story 9.1.3).
    //
    // The whole-request budget belongs to the ENGINE, not here: because the
    // engine drives the redirect loop, each hop is a separate call, so a
    // per-call limit alone would let a 20-hop chain run 20x its length. The
    // engine sets a deadline for the chain and passes what remains of it on
    // every hop, which is why these shrink as a chain progresses.
    long connect_timeout_ms = 0;
    long total_timeout_ms = 0;
};

struct NetworkResponse {
    std::string url;
    std::string effective_url;
    std::string body;
    long status = 0;
    NetworkError error = NetworkError::None;
    // Response headers as received. Repeated fields (Set-Cookie) are preserved
    // individually — see HttpHeaders.
    Core::HttpHeaders headers;
};

class INetwork {
public:
    virtual ~INetwork() = default;

    // Fetch the resource at |url| and deliver the raw body to |callback|.
    // Implementations may complete synchronously or asynchronously.
    // If redirects occur, implementations should fill effective_url with the final URL.
    virtual void get(const std::string& url, std::function<void(NetworkResponse)> callback,
                     const NetworkRequestOptions& options = {}) = 0;
    virtual void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
                      const NetworkRequestOptions& options = {}) = 0;
    // Release any background resources (threads, handles, etc).
    // Implementations must ensure no callbacks run after shutdown() returns.
    virtual void shutdown() = 0;
};

using NetworkPtr = std::unique_ptr<INetwork>;

}  // namespace Hummingbird
