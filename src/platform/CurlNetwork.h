#pragma once

#include <atomic>
#include <functional>
#include <string>

#include "core/platform_api/INetwork.h"
#include "platform/NetworkThreadPool.h"

class CurlNetwork : public INetwork {
public:
    CurlNetwork();
    ~CurlNetwork() override;

    void get(const std::string& url, std::function<void(NetworkResponse)> callback) override;

    void shutdown() override;

    static constexpr const char* accept_encoding() { return kAcceptEncoding; }

    bool ok() const { return m_initialized.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> m_initialized{false};
    Hummingbird::Platform::NetworkThreadPool thread_pool_;

    // libcurl global lifetime management (process-wide)
    static std::atomic<int> s_instances;
    static std::mutex s_global_mutex;

    static constexpr const char* kAcceptEncoding = "";
};
