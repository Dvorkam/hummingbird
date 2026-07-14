#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
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
// The top-level directory a src-relative path belongs to (its "package").
std::string module_of(std::string_view relative) {
    const auto slash = relative.find('/');
    return std::string(slash == std::string_view::npos ? relative : relative.substr(0, slash));
}

using ModuleGraph = std::map<std::string, std::set<std::string>>;

// Builds the package dependency graph from project includes across src/: an
// edge module_a -> module_b means a file in module_a includes a header in
// module_b (self-edges excluded).
ModuleGraph build_src_module_graph() {
    ModuleGraph graph;
    const std::filesystem::path root = "src";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || !has_cpp_extension(entry.path())) {
            continue;
        }
        const auto relative = std::filesystem::relative(entry.path(), root).generic_string();
        const std::string src_module = module_of(relative);
        graph.try_emplace(src_module);  // ensure isolated modules appear as nodes

        const auto content = read_file(entry.path());
        std::string line;
        auto handle_line = [&]() {
            const auto include_path = parse_project_include(line);
            if (!include_path.empty()) {
                const std::string dep_module = module_of(include_path);
                if (dep_module != src_module) {
                    graph[src_module].insert(dep_module);
                }
            }
        };
        for (char ch : content) {
            if (ch == '\n') {
                handle_line();
                line.clear();
            } else {
                line.push_back(ch);
            }
        }
        handle_line();
    }
    return graph;
}

// Returns a cyclic path (e.g. {"style","layout","style"}) if the graph contains
// a package-level cycle, or nullopt if it is acyclic. Deterministic: nodes and
// edges are iterated in sorted order.
std::optional<std::vector<std::string>> find_package_cycle(const ModuleGraph& graph) {
    enum Color { White, Gray, Black };
    std::map<std::string, Color> color;
    std::vector<std::string> stack;
    std::optional<std::vector<std::string>> cycle;

    std::function<bool(const std::string&)> visit = [&](const std::string& node) {
        color[node] = Gray;
        stack.push_back(node);
        auto it = graph.find(node);
        if (it != graph.end()) {
            for (const auto& next : it->second) {
                if (!graph.count(next)) {
                    continue;  // edge to something outside src/ (defensive)
                }
                if (color[next] == Gray) {
                    auto start = std::find(stack.begin(), stack.end(), next);
                    cycle = std::vector<std::string>(start, stack.end());
                    cycle->push_back(next);
                    return true;
                }
                if (color[next] == White && visit(next)) {
                    return true;
                }
            }
        }
        stack.pop_back();
        color[node] = Black;
        return false;
    };

    for (const auto& [node, _] : graph) {
        if (color[node] == White && visit(node)) {
            return cycle;
        }
    }
    return std::nullopt;
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

// The layering rules above forbid depending on higher layers, but they permit
// cycles among peer packages (e.g. style/ may include layout/ and layout/ may
// include style/). This guards the package graph against any such cycle so a
// refactor that introduces one fails CI deterministically.
TEST(DependencyFirewallTest, HasNoPackageDependencyCycles) {
    const ModuleGraph graph = build_src_module_graph();
    ASSERT_FALSE(graph.empty()) << "no src modules discovered; run from the repo root";

    const auto cycle = find_package_cycle(graph);
    if (cycle) {
        std::string path;
        for (size_t i = 0; i < cycle->size(); ++i) {
            path.append((*cycle)[i]);
            if (i + 1 < cycle->size()) {
                path.append(" -> ");
            }
        }
        FAIL() << "Package dependency cycle detected: " << path
               << "\nBreak the cycle (invert a dependency behind an interface, or move the shared type down a layer).";
    }
}

// Guards the guard: the detector must actually flag a cycle and clear a DAG.
TEST(DependencyFirewallTest, CycleDetectorDistinguishesCyclicFromAcyclic) {
    const ModuleGraph acyclic = {{"a", {"b", "c"}}, {"b", {"c"}}, {"c", {}}};
    EXPECT_FALSE(find_package_cycle(acyclic).has_value());

    const ModuleGraph cyclic = {{"a", {"b"}}, {"b", {"c"}}, {"c", {"a"}}};
    const auto found = find_package_cycle(cyclic);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->front(), found->back());  // path closes on itself
    EXPECT_GE(found->size(), 3u);
}
