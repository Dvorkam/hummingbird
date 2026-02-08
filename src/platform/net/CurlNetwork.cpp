#include "platform/net/CurlNetwork.h"

#include <curl/curl.h>
#include <stddef.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <ostream>
#include <utility>
#include <vector>

#include "core/utils/Log.h"
#include "platform/net/NetworkRequestUtils.h"

namespace Hummingbird::Platform {

std::atomic<int> CurlNetwork::s_instances{0};
std::mutex CurlNetwork::s_global_mutex;

namespace {
struct TlsConfig {
    bool insecure = false;
    std::string ca_bundle;
    std::string ca_path;
};

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string env_or_empty(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !*value) return {};
    return value;
}

bool env_truthy(const std::string& value) {
    if (value.empty()) return false;
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

bool file_exists(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool dir_exists(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

TlsConfig detect_tls_config() {
    TlsConfig config;
    config.insecure = env_truthy(env_or_empty("HB_TLS_INSECURE"));
    if (config.insecure) {
        HB_LOG_WARN("[network] TLS verification disabled via HB_TLS_INSECURE");
        return config;
    }

    std::string ca_bundle = env_or_empty("CURL_CA_BUNDLE");
    if (file_exists(ca_bundle)) {
        config.ca_bundle = std::move(ca_bundle);
    }

    if (config.ca_bundle.empty()) {
        std::string ssl_cert_file = env_or_empty("SSL_CERT_FILE");
        if (file_exists(ssl_cert_file)) {
            config.ca_bundle = std::move(ssl_cert_file);
        }
    }

    std::string ca_path = env_or_empty("SSL_CERT_DIR");
    if (dir_exists(ca_path)) {
        config.ca_path = std::move(ca_path);
    }

    if (config.ca_bundle.empty()) {
        const std::vector<const char*> bundle_candidates = {
            "/etc/ssl/certs/ca-certificates.crt",
            "/etc/pki/tls/certs/ca-bundle.crt",
            "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
            "/etc/ssl/cert.pem",
        };
        for (const char* candidate : bundle_candidates) {
            if (file_exists(candidate)) {
                config.ca_bundle = candidate;
                break;
            }
        }
    }

    if (config.ca_path.empty()) {
        const std::vector<const char*> path_candidates = {
            "/etc/ssl/certs",
            "/etc/pki/tls/certs",
        };
        for (const char* candidate : path_candidates) {
            if (dir_exists(candidate)) {
                config.ca_path = candidate;
                break;
            }
        }
    }

    return config;
}

const TlsConfig& tls_config() {
    static const TlsConfig config = detect_tls_config();
    return config;
}

bool is_tls_verification_error(CURLcode code) {
    if (code == CURLE_PEER_FAILED_VERIFICATION) return true;
    if (code == CURLE_SSL_CACERT) return true;
    if (code == CURLE_SSL_CERTPROBLEM) return true;
    if (code == CURLE_SSL_INVALIDCERTSTATUS) return true;
    if (code == CURLE_SSL_ISSUER_ERROR) return true;
    return false;
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

void CurlNetwork::get(const std::string& url, std::function<void(NetworkResponse)> callback,
                      const NetworkRequestOptions& options) {
    if (!ok()) {
        if (callback) callback(Hummingbird::Platform::make_response(url));
        return;
    }
    if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), callback, url)) return;

    // Move callback once, and never touch the moved-from original again.
    auto cb = std::move(callback);

    const bool allow_insecure = options.allow_insecure;
    thread_pool_.submit([url, cb = std::move(cb), this, allow_insecure]() mutable {
        if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), cb, url)) return;
        std::string body;
        NetworkResponse response = Hummingbird::Platform::make_response(url);
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

        const TlsConfig& tls = tls_config();
        const bool allow_insecure_request = tls.insecure || allow_insecure;
        if (allow_insecure_request) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        } else {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
            if (!tls.ca_bundle.empty()) {
                curl_easy_setopt(curl, CURLOPT_CAINFO, tls.ca_bundle.c_str());
            }
            if (!tls.ca_path.empty()) {
                curl_easy_setopt(curl, CURLOPT_CAPATH, tls.ca_path.c_str());
            }
        }

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
            response.error =
                is_tls_verification_error(res) ? NetworkError::TlsVerificationFailed : NetworkError::CurlError;
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

void CurlNetwork::post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
                       const NetworkRequestOptions& options) {
    if (!ok()) {
        if (callback) callback(Hummingbird::Platform::make_response(url));
        return;
    }
    if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), callback, url)) return;

    auto cb = std::move(callback);
    const std::string body_copy(body);
    const bool allow_insecure = options.allow_insecure;
    const std::string content_type =
        options.content_type.empty() ? "application/x-www-form-urlencoded" : options.content_type;

    thread_pool_.submit([url, body_copy, cb = std::move(cb), this, allow_insecure, content_type]() mutable {
        if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), cb, url)) return;
        std::string response_body;
        NetworkResponse response = Hummingbird::Platform::make_response(url);
        CURL* curl = curl_easy_init();
        if (!curl) {
            HB_LOG_WARN("[network] curl init failed: url=" << url);
            if (cb) cb(std::move(response));
            return;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, accept_encoding());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hummingbird/0.2");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_copy.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_copy.size()));

        struct curl_slist* headers = nullptr;
        const std::string content_type_header = "Content-Type: " + content_type;
        headers = curl_slist_append(headers, content_type_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        const TlsConfig& tls = tls_config();
        const bool allow_insecure_request = tls.insecure || allow_insecure;
        if (allow_insecure_request) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        } else {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
            if (!tls.ca_bundle.empty()) {
                curl_easy_setopt(curl, CURLOPT_CAINFO, tls.ca_bundle.c_str());
            }
            if (!tls.ca_path.empty()) {
                curl_easy_setopt(curl, CURLOPT_CAPATH, tls.ca_path.c_str());
            }
        }

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);

        CURLcode res = curl_easy_perform(curl);
        long status = 0;
        long ssl_verify_result = 0;
        std::string effective_url;
        std::string response_content_type;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &ssl_verify_result);
        char* content_type_ptr = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type_ptr);
        if (content_type_ptr) {
            response_content_type = content_type_ptr;
        }
        char* effective = nullptr;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective);
        if (effective) {
            effective_url = effective;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            response.error =
                is_tls_verification_error(res) ? NetworkError::TlsVerificationFailed : NetworkError::CurlError;
            HB_LOG_WARN("[network] curl failed: url=" << url << " code=" << res << " err=" << curl_easy_strerror(res)
                                                      << " status=" << status << " ssl_verify=" << ssl_verify_result
                                                      << " effective=" << effective_url << " content_type="
                                                      << response_content_type << " bytes=" << response_body.size());
        } else if (status >= 400) {
            HB_LOG_WARN("[network] http error: url=" << url << " status=" << status << " effective=" << effective_url
                                                     << " content_type=" << response_content_type
                                                     << " bytes=" << response_body.size());
        }

        if (res == CURLE_OK || !response_body.empty()) {
            response.body = std::move(response_body);
            response.status = status;
            response.effective_url = std::move(effective_url);
        }
        if (cb) cb(std::move(response));
    });
}

}  // namespace Hummingbird::Platform
