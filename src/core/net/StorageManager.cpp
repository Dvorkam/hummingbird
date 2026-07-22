#include "core/net/StorageManager.h"

#include <cstdlib>
#include <fstream>
#include <string>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

namespace Hummingbird::Core {

namespace {
// A version tag, like the cookie jar's, so a future format change is detected
// rather than misparsed.
constexpr std::string_view kFileHeader = "HBSTORAGE\t1";

// localStorage values are arbitrary strings — JSON, HTML, anything — so unlike
// cookie values they routinely contain tabs and newlines. Escape them so the
// TSV line format stays unambiguous; the file remains mostly human-readable,
// which is the point of not reaching for a binary store yet.
std::string escape(std::string_view field) {
    std::string out;
    out.reserve(field.size());
    for (char c : field) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

// Returns false on a malformed escape (trailing backslash), so a corrupt line is
// skipped rather than silently truncated.
bool unescape(std::string_view field, std::string& out) {
    out.clear();
    out.reserve(field.size());
    for (size_t i = 0; i < field.size(); ++i) {
        if (field[i] != '\\') {
            out.push_back(field[i]);
            continue;
        }
        if (++i >= field.size()) return false;
        switch (field[i]) {
            case '\\': out.push_back('\\'); break;
            case 't': out.push_back('\t'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            default: return false;
        }
    }
    return true;
}
}  // namespace

std::filesystem::path StorageManager::default_dir() {
    if (const char* configured = std::getenv("HB_STORAGE_DIR"); configured && configured[0]) {
        return std::filesystem::path(configured);
    }
    return Utils::resolve_asset_path("assets/config/storage");
}

std::filesystem::path StorageManager::path_for(const Origin& origin) const {
    return dir_ / (origin.key() + ".tsv");
}

StorageArea& StorageManager::area_for(const Origin& origin) {
    const std::string key = origin.key();
    auto [it, inserted] = areas_.try_emplace(key);
    if (inserted) {
        // First touch this session: pull whatever is on disk into the new store.
        load_origin(origin, it->second);
    }
    return it->second;
}

void StorageManager::load_origin(const Origin& origin, StorageArea& area) const {
    std::ifstream file(path_for(origin), std::ios::binary);
    if (!file) {
        return;  // No file yet: an origin with no stored data.
    }

    std::string line;
    if (!std::getline(file, line)) {
        return;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != kFileHeader) {
        HB_LOG_WARN("[storage] unrecognized file format, starting empty: " << origin.serialize());
        return;
    }

    size_t skipped = 0;
    std::string raw_key;
    std::string raw_value;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            ++skipped;
            continue;
        }
        if (!unescape(std::string_view(line).substr(0, tab), raw_key) ||
            !unescape(std::string_view(line).substr(tab + 1), raw_value)) {
            ++skipped;
            continue;
        }
        // load_entry bypasses the quota so on-disk data is always honored, and
        // skips a duplicate key so a corrupt second copy cannot shadow the first.
        area.load_entry(raw_key, raw_value);
    }
    if (skipped > 0) {
        HB_LOG_WARN("[storage] skipped " << skipped << " malformed line(s) for " << origin.serialize());
    }
}

namespace {
// Writes one area to `path`, or deletes the file when the area is empty so an
// emptied store genuinely reclaims the origin rather than leaving a header-only
// husk. Returns true when a file was written (not when one was deleted).
bool write_area_file(const std::filesystem::path& path, const StorageArea& area) {
    if (area.length() == 0) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        HB_LOG_WARN("[storage] could not write " << path.string());
        return false;
    }
    file << kFileHeader << '\n';
    for (const auto& entry : area.entries()) {
        file << escape(entry.key) << '\t' << escape(entry.value) << '\n';
    }
    return true;
}
}  // namespace

void StorageManager::save_origin(const Origin& origin) const {
    auto it = areas_.find(origin.key());
    if (it == areas_.end()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    write_area_file(path_for(origin), it->second);
}

size_t StorageManager::save_all() const {
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    size_t written = 0;
    for (const auto& [origin_key, area] : areas_) {
        if (write_area_file(dir_ / (origin_key + ".tsv"), area)) {
            ++written;
        }
    }
    return written;
}

}  // namespace Hummingbird::Core
