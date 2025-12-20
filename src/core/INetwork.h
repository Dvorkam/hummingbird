#pragma once

#include <functional>
#include <memory>
#include <string>

class INetwork {
public:
    virtual ~INetwork() = default;

    // Fetch the resource at |url| and deliver the raw body to |callback|.
    // Implementations may complete synchronously or asynchronously.
    virtual void get(const std::string& url, std::function<void(std::string)> callback) = 0;
};

using NetworkPtr = std::unique_ptr<INetwork>;
