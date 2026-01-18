#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace Hummingbird::Core {

class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t bytes, size_t max_blocks = 0);
    ~ArenaAllocator();

    // Allocate memory from the arena. Returns nullptr and marks failed on out-of-budget.
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    // No deallocation of individual objects, only reset the whole arena
    void reset();
    bool failed() const { return m_failed; }

private:
    struct Block {
        std::vector<char> buffer;
        size_t offset = 0;
    };

    size_t m_default_block_size = 0;
    size_t m_max_blocks = 0;
    std::vector<Block> m_blocks;
    bool m_failed = false;
};

template <typename T>
struct ArenaDeleter {
    void operator()(T* ptr) const noexcept {
        if (ptr) {
            std::destroy_at(ptr);
        }
    }
};

template <typename T>
using ArenaPtr = std::unique_ptr<T, ArenaDeleter<T>>;

template <typename T, typename... Args>
T* arena_new(ArenaAllocator& arena, Args&&... args) {
    // Centralized placement-new for arena-backed objects; keep this the only site that constructs in arena memory.
    void* mem = arena.allocate(sizeof(T), alignof(T));
    if (!mem) {
        return nullptr;
    }
    return new (mem) T(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
ArenaPtr<T> make_arena_ptr(ArenaAllocator& arena, Args&&... args) {
    return ArenaPtr<T>(arena_new<T>(arena, std::forward<Args>(args)...));
}

}  // namespace Hummingbird::Core
