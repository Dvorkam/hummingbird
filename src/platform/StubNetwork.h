#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/platform_api/INetwork.h"

class StubNetwork : public INetwork {
public:
    StubNetwork() = default;
    ~StubNetwork() override { shutdown(); }

    void get(const std::string& url, std::function<void(std::string)> callback) override;

    void shutdown() override;

private:
    void join_all();

private:
    std::atomic<bool> m_stopping{false};
    std::mutex m_threads_mutex;
    std::vector<std::thread> m_threads;
};
