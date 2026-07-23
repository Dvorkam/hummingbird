#pragma once

#include <stddef.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Hummingbird::Core::Utils {

inline constexpr std::string_view kUnsupportedFontFamilyWarning = "style.unsupported-font-family";

struct CompatibilityWarningEntry {
    std::string category;
    std::string detail;
    size_t count = 0;
    size_t first_seen_order = 0;
};

// Per-document statistics for expected compatibility gaps. Callers still emit
// the first warning immediately; record() returns false for repeats so hot paths
// can suppress duplicate console output without suppressing operational warnings.
class CompatibilityWarnings {
public:
    bool record(std::string_view category, std::string_view detail) {
        ++m_total_count;

        std::string composite_key;
        composite_key.reserve(category.size() + detail.size() + 1);
        composite_key.append(category);
        composite_key.push_back('\x1f');
        composite_key.append(detail);

        auto found = m_entries.find(composite_key);
        if (found != m_entries.end()) {
            ++found->second.count;
            return false;
        }

        CompatibilityWarningEntry entry;
        entry.category = category;
        entry.detail = detail;
        entry.count = 1;
        entry.first_seen_order = m_next_order++;
        m_entries.emplace(std::move(composite_key), std::move(entry));
        return true;
    }

    std::vector<CompatibilityWarningEntry> ranked() const {
        std::vector<CompatibilityWarningEntry> entries;
        entries.reserve(m_entries.size());
        for (const auto& [key, entry] : m_entries) {
            (void)key;
            entries.push_back(entry);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.count != rhs.count) {
                return lhs.count > rhs.count;
            }
            return lhs.first_seen_order < rhs.first_seen_order;
        });
        return entries;
    }

    size_t total_count() const { return m_total_count; }
    size_t unique_count() const { return m_entries.size(); }
    bool empty() const { return m_entries.empty(); }

    void clear() {
        m_entries.clear();
        m_total_count = 0;
        m_next_order = 0;
    }

private:
    std::unordered_map<std::string, CompatibilityWarningEntry> m_entries;
    size_t m_total_count = 0;
    size_t m_next_order = 0;
};

}  // namespace Hummingbird::Core::Utils
