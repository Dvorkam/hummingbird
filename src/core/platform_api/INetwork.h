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
};

struct NetworkRequestOptions {
    bool allow_insecure = false;
    std::string content_type;
    // Extra request headers (Cookie, ...). The backend owns transport headers
    // such as Content-Type and Accept-Encoding; these are added alongside.
    Core::HttpHeaders headers;
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
