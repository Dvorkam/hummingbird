#include "platform/CurlNetwork.h"

#include <curl/curl.h>

#include <utility>

std::atomic<int> CurlNetwork::s_instances{0};
std::mutex CurlNetwork::s_global_mutex;

namespace {
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
}  // namespace

CurlNetwork::CurlNetwork() {
    // Ensure curl_global_init is done once.
    {
        std::lock_guard<std::mutex> lg(s_global_mutex);
        const int n = ++s_instances;
        if (n == 1) {
            if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
                // Roll back instance count on failure.
                --s_instances;
                m_initialized.store(false, std::memory_order_relaxed);
                return;
            }
        }
    }

    m_initialized.store(true, std::memory_order_relaxed);
}

CurlNetwork::~CurlNetwork() {
    shutdown();

    if (m_initialized.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lg(s_global_mutex);
        const int n = --s_instances;
        if (n == 0) curl_global_cleanup();
    }
}

void CurlNetwork::shutdown() {
    // run once
    if (m_stopping.exchange(true, std::memory_order_relaxed)) return;
    join_all();
}

void CurlNetwork::join_all() {
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lg(m_threads_mutex);
        threads.swap(m_threads);
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

void CurlNetwork::get(const std::string& url, std::function<void(std::string)> callback) {
    if (!ok() || m_stopping.load(std::memory_order_relaxed)) {
        if (callback) callback({});
        return;
    }

    // Move callback once, and never touch the moved-from original again.
    auto cb = std::move(callback);

    std::thread worker([url, cb = std::move(cb), this]() mutable {
        if (m_stopping.load(std::memory_order_relaxed)) {
            if (cb) cb({});
            return;
        }

        std::string body;
        CURL* curl = curl_easy_init();
        if (!curl) {
            if (cb) cb({});
            return;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, accept_encoding());

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) body.clear();
        if (cb) cb(std::move(body));
    });

    {
        std::lock_guard<std::mutex> lg(m_threads_mutex);
        if (m_stopping.load(std::memory_order_relaxed)) {
            // We’re shutting down; just join the worker and return.
            // DO NOT call cb here — worker already did / will do it.
            if (worker.joinable()) worker.join();
            return;
        }
        m_threads.emplace_back(std::move(worker));
    }
}
