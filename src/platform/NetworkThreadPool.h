#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// Simple thread bucket for async network requests.
class NetworkThreadPool {
public:
    void shutdown();
    bool stopping() const;
    void submit(std::function<void()> task);

private:
    std::atomic<bool> stopping_{false};
    std::mutex threads_mutex_;
    std::vector<std::thread> threads_;
};
