#include "core/net/IdentityPolicyStore.h"

#include <cstdlib>
#include <fstream>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

namespace Hummingbird::Core {

namespace {
// Version tag so a future format change is detected rather than misparsed.
constexpr std::string_view kFileHeader = "HBIDENTITY\t1";
}  // namespace

IdentityMode IdentityPolicyStore::mode_for(const Origin& origin) const {
    return compatibility_origins_.count(origin.serialize()) ? IdentityMode::Compatibility : IdentityMode::Transparent;
}

void IdentityPolicyStore::set_mode(const Origin& origin, IdentityMode mode) {
    if (mode == IdentityMode::Compatibility) {
        compatibility_origins_.insert(origin.serialize());
    } else {
        compatibility_origins_.erase(origin.serialize());
    }
}

IdentityMode IdentityPolicyStore::toggle(const Origin& origin) {
    const IdentityMode next =
        mode_for(origin) == IdentityMode::Compatibility ? IdentityMode::Transparent : IdentityMode::Compatibility;
    set_mode(origin, next);
    return next;
}

std::filesystem::path IdentityPolicyStore::default_path() {
    if (const char* configured = std::getenv("HB_IDENTITY_FILE"); configured && configured[0]) {
        return std::filesystem::path(configured);
    }
    return Utils::resolve_asset_path("assets/config/identity.tsv");
}

size_t IdentityPolicyStore::save_to(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        HB_LOG_WARN("[identity] could not write " << path.string());
        return 0;
    }
    file << kFileHeader << '\n';
    for (const auto& origin : compatibility_origins_) {
        file << origin << '\n';
    }
    return compatibility_origins_.size();
}

size_t IdentityPolicyStore::load_from(const std::filesystem::path& path) {
    compatibility_origins_.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return 0;  // First run: no file yet is normal.
    }

    std::string line;
    if (!std::getline(file, line)) {
        return 0;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != kFileHeader) {
        HB_LOG_WARN("[identity] unrecognized file format, starting empty: " << path.string());
        return 0;
    }

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        // Only accept a well-formed origin, so a corrupt line cannot become a
        // ghost Compatibility entry for some malformed key.
        if (auto origin = Origin::parse(line)) {
            compatibility_origins_.insert(origin->serialize());
        }
    }
    return compatibility_origins_.size();
}

}  // namespace Hummingbird::Core
