#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/INetwork.h"

class CurlNetwork : public INetwork {
public:
    CurlNetwork();
    ~CurlNetwork() override;

    void get(const std::string& url, std::function<void(std::string)> callback) override;

    void shutdown();

    bool ok() const { return m_initialized.load(std::memory_order_relaxed); }

private:
    void join_all();

private:
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_stopping{false};

    std::mutex m_threads_mutex;
    std::vector<std::thread> m_threads;

    // libcurl global lifetime management (process-wide)
    static std::atomic<int> s_instances;
    static std::mutex s_global_mutex;
};
