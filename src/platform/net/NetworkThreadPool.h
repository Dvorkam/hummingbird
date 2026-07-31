#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Hummingbird::Platform {

// Simple thread bucket for async network requests.
class NetworkThreadPool {
public:
    // Joins every in-flight worker, so nothing is still touching engine state
    // after this returns. That makes it only as fast as the slowest request,
    // which is why a transfer must be able to observe `stopping()` and give up
    // — see `stopping_flag()`.
    void shutdown();
    bool stopping() const;
    void submit(std::function<void()> task);

    // The stop flag itself, for code that must poll it from inside a blocking
    // call it cannot otherwise interrupt (the curl progress callback). Checking
    // `stopping()` only before and after a transfer is not enough: the whole
    // problem is the time spent *during* one.
    const std::atomic<bool>* stopping_flag() const { return &stopping_; }

private:
    std::atomic<bool> stopping_{false};
    std::mutex threads_mutex_;
    std::vector<std::thread> threads_;
};

}  // namespace Hummingbird::Platform
