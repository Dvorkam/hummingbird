#pragma once

#include <functional>
#include <string>

#include "core/platform_api/INetwork.h"
#include "platform/NetworkThreadPool.h"

class StubNetwork : public INetwork {
public:
    StubNetwork() = default;
    ~StubNetwork() override { shutdown(); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback) override;

    void shutdown() override;

private:
    Hummingbird::Platform::NetworkThreadPool thread_pool_;
};
