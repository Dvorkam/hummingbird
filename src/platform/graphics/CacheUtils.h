#pragma once

namespace Hummingbird::Platform::CacheUtils {

template <typename Map, typename Getter>
auto find_lru_entry(Map& map, Getter get_last_used) -> typename Map::iterator {
    auto victim = map.end();
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (victim == map.end() || get_last_used(it->second) < get_last_used(victim->second)) {
            victim = it;
        }
    }
    return victim;
}

}  // namespace Hummingbird::Platform::CacheUtils
