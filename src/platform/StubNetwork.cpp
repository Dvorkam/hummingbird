#include "platform/StubNetwork.h"

void StubNetwork::shutdown() {
    if (m_stopping.exchange(true, std::memory_order_relaxed)) return;
    join_all();
}

void StubNetwork::join_all() {
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lg(m_threads_mutex);
        threads.swap(m_threads);
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

void StubNetwork::get(const std::string& url, std::function<void(std::string)> callback) {
    if (m_stopping.load(std::memory_order_relaxed)) {
        if (callback) callback({});
        return;
    }

    auto cb = std::move(callback);

    std::thread worker([url, cb = std::move(cb), this]() mutable {
        if (m_stopping.load(std::memory_order_relaxed)) {
            if (cb) cb({});
            return;
        }

        std::string body;
        if (url == "http://example.com" || url == "https://example.com") {
            body =
                "<html><body><p>Example Domain</p><p>This domain is for use in illustrative "
                "examples.</p></body></html>";
        } else {
            body = "<html><body><p>Failed to load, try to refresh?: " + url + "</p></body></html>";
        }

        if (cb) cb(std::move(body));
    });

    {
        std::lock_guard<std::mutex> lg(m_threads_mutex);
        if (m_stopping.load(std::memory_order_relaxed)) {
            if (worker.joinable()) worker.join();
            return;
        }
        m_threads.emplace_back(std::move(worker));
    }
}
