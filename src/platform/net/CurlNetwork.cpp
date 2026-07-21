#include "platform/net/CurlNetwork.h"

#include <curl/curl.h>
#include <stddef.h>

#include <functional>
#include <ostream>
#include <utility>

#include "core/utils/Log.h"
#include "platform/net/CurlTlsConfig.h"
#include "platform/net/NetworkRequestUtils.h"

namespace Hummingbird::Platform {

std::atomic<int> CurlNetwork::s_instances{0};
std::mutex CurlNetwork::s_global_mutex;

namespace {
struct CurlResponseMeta {
    long status = 0;
    long ssl_verify_result = 0;
    std::string effective_url;
    std::string content_type;
};

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// curl delivers one header line per call, including the status line and the
// blank separator; HttpHeaders::add_raw_line skips both.
size_t header_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* headers = static_cast<Core::HttpHeaders*>(userdata);
    const size_t bytes = size * nmemb;
    headers->add_raw_line(std::string_view(ptr, bytes));
    return bytes;
}

void apply_common_curl_options(CURL* curl, const std::string& url, std::string& body_buffer,
                               Core::HttpHeaders& header_buffer) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // The ENGINE follows redirects, not curl (story 8.3.1). Letting curl do it
    // hides intermediate hops, so their Set-Cookie headers never reach the jar
    // and no per-hop cookie/method policy is possible. A 3xx is therefore
    // returned here as an ordinary response, with its Location header intact,
    // and ResourceLoader::send_request drives the chain.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_buffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_buffer);
    // Refuse everything but http/https. libcurl's defaults include file://,
    // ftp:// and more, so without this a page could link (or a server redirect)
    // to `file://localhost/C:/…` and have the engine read local disk and render
    // it as a document. REDIR_PROTOCOLS is set as well even though the engine
    // now drives redirects itself — belt and braces, since a backend change
    // must not silently reopen this.
#if LIBCURL_VERSION_NUM >= 0x075500  // 7.85.0
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, CurlNetwork::accept_encoding());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hummingbird/0.2");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
}

// Appends the caller's request headers to `list`, which the caller owns and must
// free with curl_slist_free_all.
curl_slist* append_request_headers(curl_slist* list, const Core::HttpHeaders& headers) {
    for (const auto& field : headers.fields()) {
        const std::string line = field.name + ": " + field.value;
        list = curl_slist_append(list, line.c_str());
    }
    return list;
}

CurlResponseMeta collect_response_meta(CURL* curl) {
    CurlResponseMeta meta{};
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &meta.status);
    curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &meta.ssl_verify_result);

    char* content_type_ptr = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type_ptr);
    if (content_type_ptr) {
        meta.content_type = content_type_ptr;
    }

    char* effective = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective);
    if (effective) {
        meta.effective_url = effective;
    }
    return meta;
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
    Core::HttpHeaders request_headers = options.headers;
    thread_pool_.submit([url, cb = std::move(cb), this, allow_insecure,
                         request_headers = std::move(request_headers)]() mutable {
        if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), cb, url)) return;
        std::string body;
        NetworkResponse response = Hummingbird::Platform::make_response(url);
        CURL* curl = curl_easy_init();
        if (!curl) {
            HB_LOG_WARN("[network] curl init failed: url=" << url);
            if (cb) cb(std::move(response));
            return;
        }

        apply_common_curl_options(curl, url, body, response.headers);
        struct curl_slist* headers = append_request_headers(nullptr, request_headers);
        if (headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
        Hummingbird::Platform::apply_tls_options(curl, allow_insecure);

        CURLcode res = curl_easy_perform(curl);
        CurlResponseMeta meta = collect_response_meta(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            response.error =
                is_tls_verification_error(res) ? NetworkError::TlsVerificationFailed : NetworkError::CurlError;
            HB_LOG_WARN("[network] curl failed: url="
                        << url << " code=" << res << " err=" << curl_easy_strerror(res) << " status=" << meta.status
                        << " ssl_verify=" << meta.ssl_verify_result << " effective=" << meta.effective_url
                        << " content_type=" << meta.content_type << " bytes=" << body.size());
        } else if (meta.status >= 400) {
            HB_LOG_WARN("[network] http error: url=" << url << " status=" << meta.status << " effective="
                                                     << meta.effective_url << " content_type=" << meta.content_type
                                                     << " bytes=" << body.size());
        }

        if (res == CURLE_OK || !body.empty()) {
            response.body = std::move(body);
            response.status = meta.status;
            response.effective_url = std::move(meta.effective_url);
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

    Core::HttpHeaders request_headers = options.headers;
    thread_pool_.submit([url, body_copy, cb = std::move(cb), this, allow_insecure, content_type,
                         request_headers = std::move(request_headers)]() mutable {
        if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), cb, url)) return;
        std::string response_body;
        NetworkResponse response = Hummingbird::Platform::make_response(url);
        CURL* curl = curl_easy_init();
        if (!curl) {
            HB_LOG_WARN("[network] curl init failed: url=" << url);
            if (cb) cb(std::move(response));
            return;
        }

        apply_common_curl_options(curl, url, response_body, response.headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_copy.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_copy.size()));

        struct curl_slist* headers = nullptr;
        const std::string content_type_header = "Content-Type: " + content_type;
        headers = curl_slist_append(headers, content_type_header.c_str());
        headers = append_request_headers(headers, request_headers);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        Hummingbird::Platform::apply_tls_options(curl, allow_insecure);

        CURLcode res = curl_easy_perform(curl);
        CurlResponseMeta meta = collect_response_meta(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            response.error =
                is_tls_verification_error(res) ? NetworkError::TlsVerificationFailed : NetworkError::CurlError;
            HB_LOG_WARN("[network] curl failed: url="
                        << url << " code=" << res << " err=" << curl_easy_strerror(res) << " status=" << meta.status
                        << " ssl_verify=" << meta.ssl_verify_result << " effective=" << meta.effective_url
                        << " content_type=" << meta.content_type << " bytes=" << response_body.size());
        } else if (meta.status >= 400) {
            HB_LOG_WARN("[network] http error: url=" << url << " status=" << meta.status << " effective="
                                                     << meta.effective_url << " content_type=" << meta.content_type
                                                     << " bytes=" << response_body.size());
        }

        if (res == CURLE_OK || !response_body.empty()) {
            response.body = std::move(response_body);
            response.status = meta.status;
            response.effective_url = std::move(meta.effective_url);
        }
        if (cb) cb(std::move(response));
    });
}

}  // namespace Hummingbird::Platform
