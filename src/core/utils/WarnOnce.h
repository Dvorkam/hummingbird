#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace Hummingbird::Core::Utils {

class WarnOnce {
public:
    bool should_log(std::string_view key) { return m_seen.insert(std::string(key)).second; }
    void clear() { m_seen.clear(); }
    const std::unordered_set<std::string>& seen() const { return m_seen; }

private:
    std::unordered_set<std::string> m_seen;
};

}  // namespace Hummingbird::Core::Utils
