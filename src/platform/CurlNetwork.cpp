#include "platform/CurlNetwork.h"

#include <curl/curl.h>

#include <utility>

#include "core/utils/Log.h"

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
    thread_pool_.shutdown();
}

void CurlNetwork::get(const std::string& url, std::function<void(NetworkResponse)> callback) {
    if (!ok() || thread_pool_.stopping()) {
        if (callback) callback(NetworkResponse{.url = url});
        return;
    }

    // Move callback once, and never touch the moved-from original again.
    auto cb = std::move(callback);

    thread_pool_.submit([url, cb = std::move(cb), this]() mutable {
        if (thread_pool_.stopping()) {
            if (cb) cb(NetworkResponse{.url = url});
            return;
        }

        std::string body;
        NetworkResponse response;
        response.url = url;
        CURL* curl = curl_easy_init();
        if (!curl) {
            HB_LOG_WARN("[network] curl init failed: url=" << url);
            if (cb) cb(std::move(response));
            return;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, accept_encoding());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hummingbird/0.2");

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);

        CURLcode res = curl_easy_perform(curl);
        long status = 0;
        long ssl_verify_result = 0;
        std::string effective_url;
        std::string content_type;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &ssl_verify_result);
        char* content_type_ptr = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type_ptr);
        if (content_type_ptr) {
            content_type = content_type_ptr;
        }
        char* effective = nullptr;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective);
        if (effective) {
            effective_url = effective;
        }
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            HB_LOG_WARN("[network] curl failed: url=" << url << " code=" << res << " err=" << curl_easy_strerror(res)
                                                      << " status=" << status << " ssl_verify=" << ssl_verify_result
                                                      << " effective=" << effective_url
                                                      << " content_type=" << content_type << " bytes=" << body.size());
        } else if (status >= 400) {
            HB_LOG_WARN("[network] http error: url=" << url << " status=" << status << " effective=" << effective_url
                                                     << " content_type=" << content_type << " bytes=" << body.size());
        }

        if (res == CURLE_OK || !body.empty()) {
            response.body = std::move(body);
            response.status = status;
            response.effective_url = std::move(effective_url);
        }
        if (cb) cb(std::move(response));
    });
}
