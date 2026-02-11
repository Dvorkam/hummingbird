#include "platform/net/CurlTlsConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "core/utils/Log.h"

namespace Hummingbird::Platform {

namespace {

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

}  // namespace

const TlsConfig& tls_config() {
    static const TlsConfig config = detect_tls_config();
    return config;
}

void apply_tls_options(CURL* curl, bool allow_insecure) {
    const TlsConfig& tls = tls_config();
    const bool allow_insecure_request = tls.insecure || allow_insecure;
    if (allow_insecure_request) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    if (!tls.ca_bundle.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, tls.ca_bundle.c_str());
    }
    if (!tls.ca_path.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAPATH, tls.ca_path.c_str());
    }
}

bool is_tls_verification_error(CURLcode code) {
    if (code == CURLE_PEER_FAILED_VERIFICATION) return true;
    if (code == CURLE_SSL_CACERT) return true;
    if (code == CURLE_SSL_CERTPROBLEM) return true;
    if (code == CURLE_SSL_INVALIDCERTSTATUS) return true;
    if (code == CURLE_SSL_ISSUER_ERROR) return true;
    return false;
}

}  // namespace Hummingbird::Platform
