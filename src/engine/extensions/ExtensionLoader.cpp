#include "engine/extensions/ExtensionLoader.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "core/utils/AssetPath.h"

namespace Hummingbird::Engine {

namespace {
constexpr const char* kManifestFileName = "manifest.json";

std::optional<std::string> read_file_to_string(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return std::nullopt;
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void push_error(std::vector<ExtensionLoadError>* errors, std::string message, std::filesystem::path path) {
    if (!errors) return;
    errors->push_back(ExtensionLoadError{std::move(message), std::move(path)});
}

bool is_subpath(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto r = root.lexically_normal();
    auto c = candidate.lexically_normal();
    auto [it_r, it_c] = std::mismatch(r.begin(), r.end(), c.begin(), c.end());
    return it_r == r.end();
}

std::optional<std::filesystem::path> resolve_entry_path(const std::filesystem::path& root_dir, const std::string& entry,
                                                        std::vector<ExtensionLoadError>* errors) {
    std::filesystem::path entry_path(entry);
    if (entry_path.empty()) {
        push_error(errors, "background.entry is empty", root_dir);
        return std::nullopt;
    }
    if (entry_path.is_absolute()) {
        push_error(errors, "background.entry must be a relative path", root_dir / entry_path);
        return std::nullopt;
    }

    auto combined = (root_dir / entry_path).lexically_normal();
    if (!is_subpath(root_dir, combined)) {
        push_error(errors, "background.entry must not escape extension root", combined);
        return std::nullopt;
    }
    if (!std::filesystem::exists(combined)) {
        push_error(errors, "background.entry does not exist", combined);
        return std::nullopt;
    }
    if (!std::filesystem::is_regular_file(combined)) {
        push_error(errors, "background.entry must be a file", combined);
        return std::nullopt;
    }
    return combined;
}

}  // namespace

std::filesystem::path default_extensions_root() {
    if (const char* dir = std::getenv("HB_EXTENSIONS_DIR"); dir && *dir) {
        return std::filesystem::path(dir);
    }
    return Hummingbird::Core::Utils::resolve_asset_path("assets/extensions");
}

std::vector<LoadedExtension> load_extensions_from_root(const std::filesystem::path& root_dir,
                                                       std::vector<ExtensionLoadError>* errors) {
    std::vector<LoadedExtension> loaded;

    std::error_code ec;
    if (!std::filesystem::exists(root_dir, ec)) {
        push_error(errors, "extensions root does not exist", root_dir);
        return loaded;
    }
    if (!std::filesystem::is_directory(root_dir, ec)) {
        push_error(errors, "extensions root is not a directory", root_dir);
        return loaded;
    }

    std::vector<std::filesystem::path> candidates;
    for (const auto& entry : std::filesystem::directory_iterator(root_dir, ec)) {
        if (ec) break;
        if (!entry.is_directory(ec)) continue;
        candidates.push_back(entry.path());
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.filename().string() < b.filename().string(); });

    for (const auto& ext_root : candidates) {
        auto manifest_path = ext_root / kManifestFileName;
        auto contents = read_file_to_string(manifest_path);
        if (!contents) {
            push_error(errors, "missing manifest.json", manifest_path);
            continue;
        }

        ManifestParseError parse_error;
        auto manifest = parse_extension_manifest(*contents, &parse_error);
        if (!manifest) {
            push_error(errors,
                       "invalid manifest.json: " + parse_error.message + " (offset " +
                           std::to_string(parse_error.offset) + ")",
                       manifest_path);
            continue;
        }

        auto entry_path = resolve_entry_path(ext_root, manifest->background_entry, errors);
        if (!entry_path) continue;

        LoadedExtension out;
        out.manifest = std::move(*manifest);
        out.root_dir = ext_root;
        out.manifest_path = manifest_path;
        out.background_entry_path = std::move(*entry_path);
        loaded.push_back(std::move(out));
    }

    return loaded;
}

}  // namespace Hummingbird::Engine
