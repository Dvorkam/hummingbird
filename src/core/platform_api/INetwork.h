#pragma once

#include <functional>
#include <memory>
#include <string>

struct NetworkResponse {
    std::string url;
    std::string effective_url;
    std::string body;
    long status = 0;
};

class INetwork {
public:
    virtual ~INetwork() = default;

    // Fetch the resource at |url| and deliver the raw body to |callback|.
    // Implementations may complete synchronously or asynchronously.
    // If redirects occur, implementations should fill effective_url with the final URL.
    virtual void get(const std::string& url, std::function<void(NetworkResponse)> callback) = 0;
    // Release any background resources (threads, handles, etc).
    // Implementations must ensure no callbacks run after shutdown() returns.
    virtual void shutdown() = 0;
};

using NetworkPtr = std::unique_ptr<INetwork>;
