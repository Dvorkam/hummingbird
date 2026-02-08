#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace Hummingbird::Engine {

class ResourceSecurityPolicy final {
public:
    void allow_insecure_host(std::string_view host);
    bool is_insecure_allowed_for_url(std::string_view url) const;
    static std::string build_tls_error_body(std::string_view url);

private:
    static std::string normalize_host(std::string_view host);

    std::unordered_set<std::string> insecure_hosts_;
};

}  // namespace Hummingbird::Engine
