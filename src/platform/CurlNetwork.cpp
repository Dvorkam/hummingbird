#include "platform/CurlNetwork.h"

#include <curl/curl.h>

#include <thread>

namespace {
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
}  // namespace

CurlNetwork::CurlNetwork() {
    auto res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res == 0) {
        m_initialized = true;
    }
}

CurlNetwork::~CurlNetwork() {
    if (m_initialized) {
        curl_global_cleanup();
    }
}

void CurlNetwork::get(const std::string& url, std::function<void(std::string)> callback) {
    if (!m_initialized) {
        callback({});
        return;
    }

    std::thread([url, cb = std::move(callback)]() mutable {
        std::string body;
        CURL* curl = curl_easy_init();
        if (!curl) {
            cb(std::move(body));
            return;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            body.clear();
        }
        cb(std::move(body));
    }).detach();
}
