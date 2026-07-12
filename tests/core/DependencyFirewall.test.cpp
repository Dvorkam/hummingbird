#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct Rule {
    std::string source_prefix;
    std::vector<std::string> forbidden_prefixes;
};

bool has_cpp_extension(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    return ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".cc";
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string trim_left(std::string_view line) {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        ++i;
    }
    return std::string(line.substr(i));
}

std::string parse_project_include(std::string_view line) {
    auto trimmed = trim_left(line);
    if (!trimmed.starts_with("#include \"")) {
        return {};
    }
    const size_t start = std::string_view("#include \"").size();
    auto end = trimmed.find('"', start);
    if (end == std::string::npos || end <= start) {
        return {};
    }
    return trimmed.substr(start, end - start);
}

void collect_violations(const Rule& rule, std::vector<std::string>& violations) {
    const std::filesystem::path root = "src";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto relative = std::filesystem::relative(entry.path(), root).generic_string();
        if (!relative.starts_with(rule.source_prefix) || !has_cpp_extension(entry.path())) {
            continue;
        }

        const auto content = read_file(entry.path());
        std::string current_line;
        size_t line_number = 1;
        for (char ch : content) {
            if (ch == '\n') {
                const auto include_path = parse_project_include(current_line);
                if (!include_path.empty()) {
                    for (const auto& forbidden : rule.forbidden_prefixes) {
                        if (include_path.starts_with(forbidden)) {
                            violations.push_back(relative + ":" + std::to_string(line_number) + " includes \"" +
                                                 include_path + "\"");
                        }
                    }
                }
                current_line.clear();
                ++line_number;
            } else {
                current_line.push_back(ch);
            }
        }
        if (!current_line.empty()) {
            const auto include_path = parse_project_include(current_line);
            if (!include_path.empty()) {
                for (const auto& forbidden : rule.forbidden_prefixes) {
                    if (include_path.starts_with(forbidden)) {
                        violations.push_back(relative + ":" + std::to_string(line_number) + " includes \"" +
                                             include_path + "\"");
                    }
                }
            }
        }
    }
}
}  // namespace

TEST(DependencyFirewallTest, EnforcesKeyLayerBoundaries) {
    const std::vector<Rule> rules = {
        {"core/", {"app/", "engine/", "html/", "layout/", "renderer/", "style/", "platform/"}},
        {"html/", {"app/", "engine/", "platform/"}},
        {"style/", {"app/", "engine/", "platform/"}},
        {"layout/", {"app/", "engine/", "platform/"}},
        {"renderer/", {"app/", "engine/", "platform/"}},
        {"engine/", {"app/", "platform/"}},
        {"app/", {"platform/"}},
    };

    std::vector<std::string> violations;
    for (const auto& rule : rules) {
        collect_violations(rule, violations);
    }

    if (!violations.empty()) {
        std::string message = "Dependency firewall violations:\n";
        for (const auto& violation : violations) {
            message.append("  - ");
            message.append(violation);
            message.push_back('\n');
        }
        FAIL() << message;
    }
}
