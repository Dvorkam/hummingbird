#include "platform/NetworkThreadPool.h"

namespace Hummingbird::Platform {

void NetworkThreadPool::shutdown() {
    if (stopping_.exchange(true, std::memory_order_relaxed)) return;

    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lg(threads_mutex_);
        threads.swap(threads_);
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

bool NetworkThreadPool::stopping() const {
    return stopping_.load(std::memory_order_relaxed);
}

void NetworkThreadPool::submit(std::function<void()> task) {
    if (stopping()) return;

    std::thread worker([task = std::move(task)]() mutable { task(); });
    {
        std::lock_guard<std::mutex> lg(threads_mutex_);
        if (stopping()) {
            if (worker.joinable()) {
                worker.join();
            }
            return;
        }
        threads_.emplace_back(std::move(worker));
    }
}

}  // namespace Hummingbird::Platform
