#pragma once

#include <curl/curl.h>

#include <string>

namespace Hummingbird::Platform {

struct TlsConfig {
    bool insecure = false;
    std::string ca_bundle;
    std::string ca_path;
};

const TlsConfig& tls_config();
void apply_tls_options(CURL* curl, bool allow_insecure);
bool is_tls_verification_error(CURLcode code);

}  // namespace Hummingbird::Platform
